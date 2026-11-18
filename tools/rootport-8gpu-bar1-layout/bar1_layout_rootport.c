// SPDX-License-Identifier: GPL-2.0-only
/* BAR1 64 GiB resource layout for root-port topologies: every GPU sits
 * directly behind its own PCI root port (no PCIe switch), e.g. a dual-socket
 * server with one GPU per root port and a large (1 TiB) host prefetch window
 * per root bus.
 *
 * Run AFTER the gpu_unlock payload has advertised the 64 GiB BAR1 capability
 * (gpu_unlock.bar1_64g=1) and BEFORE any driver (nvidia) binds the GPUs.
 * The intended point is an initramfs init-premount script.
 *
 * Modes:
 *   (default)      dry-run: report every GPU's state and the planned changes
 *   commit=1       apply: extend each root port's prefetch window and assign
 *                  the GPU a 64 GiB BAR1 (+ 32 MiB BAR3)
 *
 * The GPU list is a module parameter (hex bus numbers of the VGA functions):
 *   buses="16,38,49,5a,98,b8,c8,d8"   (default = the validated 8-card host)
 *
 * Safety:
 *  - duplicate (or re-spelled) bus entries are rejected before any write;
 *  - each GPU is held with pci_dev_lock() across check+commit+rollback so a
 *    driver cannot bind mid-transaction;
 *  - on failure, partial allocations are released children-first, then the
 *    saved bridge windows are reclaimed and programmed, then the BARs are
 *    restored to their saved addresses and command registers are put back;
 *    recovery success is reported per device.
 *
 * Derived from tools/server-161-bar1-layout/bar1_layout.c (same repo), which
 * targets a 4-GPU Microchip switch topology. Same ReBAR / resource
 * assign-release discipline; different topology and failure handling.
 */
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sizes.h>

static int commit;
module_param(commit, int, 0400);
MODULE_PARM_DESC(commit, "apply the layout (default: dry-run report only)");

#define MAX_GPU 32

/* comma-separated hex bus numbers of the VGA functions to process */
static char buses_str[] = "16,38,49,5a,98,b8,c8,d8";
module_param_string(buses, buses_str, sizeof(buses_str), 0400);
MODULE_PARM_DESC(buses, "comma-separated hex PCI bus numbers of the GPUs");

static char *gpu_buses[MAX_GPU];
static int gpu_buses_n;

static struct pci_dev *gpus[MAX_GPU], *ports[MAX_GPU];
static int ctrls[MAX_GPU];
static u16 cmds[MAX_GPU];
static u32 orig_ctrl[MAX_GPU];
static struct resource orig_bar[MAX_GPU][2];  /* [i][0]=BAR1, [i][1]=BAR3 */
static struct resource orig_win[MAX_GPU];     /* port prefetch window */

/* locate the ReBAR control register entry governing BAR1 (index 1) */
static int bar1_control(struct pci_dev *dev)
{
	int cap = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_REBAR);
	u32 ctrl;
	unsigned int i, entries;

	if (!cap || pci_read_config_dword(dev, cap + PCI_REBAR_CTRL, &ctrl))
		return -ENODEV;
	entries = (ctrl & PCI_REBAR_CTRL_NBAR_MASK) >> PCI_REBAR_CTRL_NBAR_SHIFT;
	if (!entries || entries > 6)
		return -EINVAL;
	for (i = 0; i < entries; i++) {
		int pos = cap + PCI_REBAR_CTRL + 8 * i;
		if (pci_read_config_dword(dev, pos, &ctrl))
			return -EIO;
		if ((ctrl & PCI_REBAR_CTRL_BAR_IDX) == 1)
			return pos;
	}
	return -ENODEV;
}

/* keep Linux's resource tree and bridge hardware in agreement (upstream code) */
static void program_window(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
	u32 low = ((r->start >> 16) & 0xfff0) | (r->end & 0xfff00000) | 0x10001;

	pci_write_config_dword(dev, PCI_PREF_LIMIT_UPPER32, 0);
	pci_write_config_dword(dev, PCI_PREF_MEMORY_BASE, low);
	pci_write_config_dword(dev, PCI_PREF_BASE_UPPER32, upper_32_bits(r->start));
	pci_write_config_dword(dev, PCI_PREF_LIMIT_UPPER32, upper_32_bits(r->end));
}

/* upstream semantics: start carries the 64 GiB ALIGNMENT hint (STARTALIGN);
 * the allocator picks the actual base inside the (1 TiB) root window. */
static void window(struct pci_dev *dev, resource_size_t size)
{
	struct resource *r = &dev->resource[PCI_BRIDGE_PREF_MEM_WINDOW];

	r->start = SZ_64G;
	r->end = r->start + size - 1;
	r->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH | IORESOURCE_MEM_64 |
		   IORESOURCE_STARTALIGN | IORESOURCE_UNSET;
}

static void set_bar(struct pci_dev *gpu, int bar, resource_size_t size)
{
	struct resource *r = &gpu->resource[bar];

	r->start = 0;
	r->end = size - 1;
	r->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH | IORESOURCE_MEM_64 |
		   IORESOURCE_SIZEALIGN | IORESOURCE_UNSET | 0xc;
	/* 0xc: config-space BAR type bits (64-bit + prefetchable). NVIDIA's
	 * nv_pci_validate_bars() detects 64-bit BARs via those low bits inside
	 * pci_resource_flags(); without them it mistakes the upper half of a
	 * 64-bit BAR for an invalid BAR and refuses to probe. */
}

/* Ordered rollback of a failed commit. Returns the number of GPUs whose
 * original configuration was fully restored. */
static int rollback(int n)
{
	int i, bar, ok = 0;

	/* 1) release partial allocations children-first (GPU BARs, then windows) */
	for (i = 0; i < n; i++)
		for (bar = 1; bar <= 3; bar += 2)
			if (gpus[i]->resource[bar].parent)
				pci_release_resource(gpus[i], bar);
	for (i = 0; i < n; i++)
		if (ports[i]->resource[PCI_BRIDGE_PREF_MEM_WINDOW].parent)
			pci_release_resource(ports[i], PCI_BRIDGE_PREF_MEM_WINDOW);

	/* 2) reclaim the saved bridge windows (and program the hardware) */
	for (i = 0; i < n; i++) {
		struct resource *r = &ports[i]->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
		struct resource *o = &orig_win[i];

		r->start = o->start;
		r->end = o->end;
		r->flags = (o->flags & ~IORESOURCE_UNSET) | IORESOURCE_WINDOW;
		if (pci_assign_resource(ports[i], PCI_BRIDGE_PREF_MEM_WINDOW)) {
			pr_err("bar1_layout: rollback: window reclaim failed on %s\n",
			       pci_name(ports[i]));
			continue;
		}
		program_window(ports[i]);

		/* 3) restore the GPU BARs at their saved addresses */
		for (bar = 1; bar <= 3; bar += 2) {
			struct resource *g = &gpus[i]->resource[bar];
			struct resource *s = &orig_bar[i][bar >> 1];

			g->start = s->start;
			g->end = s->end;
			g->flags = (s->flags & ~IORESOURCE_UNSET) | IORESOURCE_WINDOW;
			if (pci_assign_resource(gpus[i], bar)) {
				pr_err("bar1_layout: rollback: BAR%d restore failed on %s\n",
				       bar, gpu_buses[i]);
				break;
			}
		}
		if (bar > 3) {
			/* 4) put the command register back (re-enables decoding) */
			pci_write_config_word(gpus[i], PCI_COMMAND, cmds[i]);
			ok++;
		}
	}
	return ok;
}

static int __init layout_init(void)
{
	int i, j, n = 0, ret = 0, recovered;

	/* parse the bus list (hex tokens) */
	{
		char *p = buses_str, *tok;
		while ((tok = strsep(&p, ",")) != NULL && gpu_buses_n < MAX_GPU) {
			if (!*tok)
				continue;
			gpu_buses[gpu_buses_n++] = tok;
		}
	}
	if (!gpu_buses_n) {
		pr_err("bar1_layout: empty buses= parameter\n");
		return -EINVAL;
	}
	pr_info("bar1_layout: %s on %d GPUs (%s)\n", commit ? "COMMIT" : "dry-run",
		gpu_buses_n, buses_str);

	/* resolve every bus to (gpu, port), reject duplicates/re-spellings */
	for (i = 0; i < gpu_buses_n; i++) {
		int bus = simple_strtoul(gpu_buses[i], NULL, 16);
		struct pci_dev *gpu, *port;
		u16 cmd = 0xffff;
		int ctrl;

		gpu = pci_get_domain_bus_and_slot(0, bus, PCI_DEVFN(0, 0));
		if (!gpu || gpu->vendor != 0x10de || gpu->device != 0x2684) {
			pr_err("bar1_layout: %s:00.0 not a 10de:2684\n", gpu_buses[i]);
			if (gpu)
				pci_dev_put(gpu);
			ret = -ENODEV;
			goto out;
		}
		port = gpu->bus->self;
		if (!port) {
			pr_err("bar1_layout: %s: no parent port\n", gpu_buses[i]);
			pci_dev_put(gpu);
			ret = -ENODEV;
			goto out;
		}
		for (j = 0; j < n; j++) {
			if (gpus[j] == gpu || ports[j] == port) {
				pr_err("bar1_layout: duplicate GPU/port via %s\n",
				       gpu_buses[i]);
				pci_dev_put(gpu);
				ret = -EINVAL;
				goto out;
			}
		}
		if (pci_read_config_word(gpu, PCI_COMMAND, &cmd) || cmd == 0xffff) {
			pr_err("bar1_layout: %s: config read failed\n", gpu_buses[i]);
			pci_dev_put(gpu);
			ret = -EIO;
			goto out;
		}
		ctrl = bar1_control(gpu);
		gpus[n] = gpu; ports[n] = port; ctrls[n] = ctrl; cmds[n] = cmd;
		orig_bar[n][0] = gpu->resource[1];
		orig_bar[n][1] = gpu->resource[3];
		orig_win[n] = port->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
		if (ctrl > 0)
			pci_read_config_dword(gpu, ctrl, &orig_ctrl[n]);
		pr_info("bar1_layout: %s:00.0 BAR1 %pR (64GiB possible: %s) port %s window %pR\n",
			gpu_buses[i], &gpu->resource[1],
			(pci_rebar_get_possible_sizes(gpu, 1) & BIT(16)) ? "yes" : "NO",
			pci_name(port), &port->resource[PCI_BRIDGE_PREF_MEM_WINDOW]);
		n++;
	}
	if (!commit) {
		pr_info("bar1_layout: dry-run complete; %d GPUs probed\n", n);
		goto out;
	}

	for (i = 0; i < n; i++) {
		if (ctrls[i] < 0 || !(pci_rebar_get_possible_sizes(gpus[i], 1) & BIT(16))) {
			pr_err("bar1_layout: %s: 64 GiB not possible; unlock first\n",
			       gpu_buses[i]);
			ret = -EINVAL;
			goto out;
		}
		if (gpus[i]->driver || (cmds[i] & PCI_COMMAND_MASTER)) {
			pr_err("bar1_layout: %s: bound or bus-mastering\n", gpu_buses[i]);
			ret = -EBUSY;
			goto out;
		}
	}

	pci_lock_rescan_remove();
	/* hold every target GPU across check+commit+rollback: a driver cannot
	 * bind while we hold the device lock (nvidia's probe waits on it). */
	for (i = 0; i < n; i++)
		pci_dev_lock(gpus[i]);

	/* stop the GPUs from decoding while we move their BARs */
	for (i = 0; i < n; i++)
		pci_write_config_word(gpus[i], PCI_COMMAND,
				      cmds[i] & ~PCI_COMMAND_MEMORY);
	/* release children first, then the port windows */
	for (i = 0; i < n; i++) {
		pci_release_resource(gpus[i], 1);
		pci_release_resource(gpus[i], 3);
	}
	for (i = 0; i < n; i++)
		pci_release_resource(ports[i], PCI_BRIDGE_PREF_MEM_WINDOW);
	/* program the 64 GiB size before assignment */
	for (i = 0; i < n; i++) {
		u32 v, a;
		pci_read_config_dword(gpus[i], ctrls[i], &v);
		v = (v & ~PCI_REBAR_CTRL_BAR_SIZE) | (16 << PCI_REBAR_CTRL_BAR_SHIFT);
		pci_write_config_dword(gpus[i], ctrls[i], v);
		pci_read_config_dword(gpus[i], ctrls[i], &a);
		if (a != v) {
			pr_err("bar1_layout: %s: size control refused\n", gpu_buses[i]);
			ret = -EIO;
			goto fail;
		}
	}
	for (i = 0; i < n; i++) {
		window(ports[i], 2 * (resource_size_t)SZ_64G);
		ret = pci_assign_resource(ports[i], PCI_BRIDGE_PREF_MEM_WINDOW);
		if (ret) {
			pr_err("bar1_layout: port %s window assign failed %d\n",
			       pci_name(ports[i]), ret);
			goto fail;
		}
	}
	for (i = 0; i < n; i++) {
		for (j = 1; j <= 3; j += 2) {
			set_bar(gpus[i], j, j == 1 ? SZ_64G : SZ_32M);
			ret = pci_assign_resource(gpus[i], j);
			if (ret) {
				pr_err("bar1_layout: %s BAR%d assign failed %d\n",
				       gpu_buses[i], j, ret);
				goto fail;
			}
		}
	}
	for (i = 0; i < n; i++) {
		program_window(ports[i]);
		pci_write_config_word(gpus[i], PCI_COMMAND, cmds[i]);
		pr_info("bar1_layout: %s:00.0 BAR1=%pR BAR3=%pR win=%pR\n",
			gpu_buses[i], &gpus[i]->resource[1], &gpus[i]->resource[3],
			&ports[i]->resource[PCI_BRIDGE_PREF_MEM_WINDOW]);
	}
	pr_info("bar1_layout: committed 64 GiB BAR1 on %d GPUs\n", n);
	goto unlock;

fail:
	recovered = rollback(n);
	if (recovered == n)
		pr_err("bar1_layout: commit failed; stock layout restored on %d GPUs\n",
		       recovered);
	else
		pr_err("bar1_layout: commit failed; recovery incomplete (%d/%d); affected GPUs stay decode-disabled\n",
		       recovered, n);
unlock:
	for (i = 0; i < n; i++)
		pci_dev_unlock(gpus[i]);
	pci_unlock_rescan_remove();
out:
	for (i = 0; i < n; i++)
		pci_dev_put(gpus[i]);
	return ret;
}

static void __exit layout_exit(void) { }
module_init(layout_init);
module_exit(layout_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("root-port topology 64 GiB BAR1 resource layout (gpu-unlock 48G/64G companion)");
