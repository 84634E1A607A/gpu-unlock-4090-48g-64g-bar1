// SPDX-License-Identifier: GPL-2.0-only
/* gpu-server-161 only: one 4-GPU Microchip switch under root 97:02.0/c9:02.0.
 * Run after gpu_unlock and before NVIDIA. Avoid the kernel's undersized
 * nested-bridge packing by assigning a 128 GiB window to each populated port.
 * BAR0, ROM, audio BARs, non-prefetchable windows and link settings stay put.
 */
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sizes.h>

static int root_bus = -1;
module_param(root_bus, int, 0400);
MODULE_PARM_DESC(root_bus, "server-161 GPU root bus: 0x97 or 0xc9");
static struct pci_dev *devices[32];
static u16 commands[32];
static int controls[32];
static unsigned int count, gpus;
static int collect_error;

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

static int collect(struct pci_dev *dev, void *unused)
{
	u16 cmd;
	int ctrl = 0;

	if (count == ARRAY_SIZE(devices))
		return collect_error = -E2BIG;
	if (pci_read_config_word(dev, PCI_COMMAND, &cmd) || cmd == 0xffff)
		return collect_error = -EIO;
	if (dev->subordinate) {
		if (dev->vendor != 0x11f8 || dev->device != 0x4000)
			return collect_error = -ENODEV;
	} else if (dev->vendor == 0x10de && dev->device == 0x2684) {
		if (dev->driver || (cmd & PCI_COMMAND_MASTER) || dev->resource[0].child ||
		    resource_size(&dev->resource[3]) != SZ_32M ||
		    !(pci_rebar_get_possible_sizes(dev, 1) & BIT_ULL(16)))
			return collect_error = -EBUSY;
		ctrl = bar1_control(dev);
		if (ctrl < 0)
			return collect_error = ctrl;
		gpus++;
	} else if (dev->vendor != 0x10de || dev->device != 0x22ba) {
		return collect_error = -ENODEV;
	}
	devices[count] = dev;
	commands[count] = cmd;
	controls[count++] = ctrl;
	return 0;
}

static void window(struct pci_dev *dev, resource_size_t size)
{
	struct resource *r = &dev->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
	r->start = SZ_64G;
	r->end = r->start + size - 1;
	r->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH | IORESOURCE_MEM_64 |
		IORESOURCE_STARTALIGN | IORESOURCE_UNSET;
}

/* x86 server-161 has identical CPU and PCI bus addresses. Keep Linux's
 * resource tree and bridge hardware in agreement after pci_assign_resource(). */
static void program_window(struct pci_dev *dev)
{
	struct resource *r = &dev->resource[PCI_BRIDGE_PREF_MEM_WINDOW];
	u32 low = ((r->start >> 16) & 0xfff0) | (r->end & 0xfff00000) | 0x10001;

	pci_write_config_dword(dev, PCI_PREF_LIMIT_UPPER32, 0);
	pci_write_config_dword(dev, PCI_PREF_MEMORY_BASE, low);
	pci_write_config_dword(dev, PCI_PREF_BASE_UPPER32, upper_32_bits(r->start));
	pci_write_config_dword(dev, PCI_PREF_LIMIT_UPPER32, upper_32_bits(r->end));
}

static int __init layout_init(void)
{
	struct pci_dev *root;
	int ret = 0, n, i;

	if (root_bus != 0x97 && root_bus != 0xc9)
		return -EINVAL;
	root = pci_get_domain_bus_and_slot(0, root_bus, PCI_DEVFN(2, 0));
	if (!root || !root->subordinate) {
		pci_dev_put(root);
		return -ENODEV;
	}
	pci_lock_rescan_remove();
	pci_walk_bus(root->subordinate, collect, NULL);
	if (collect_error || gpus != 4) {
		ret = collect_error ? collect_error : -ENODEV;
		goto out;
	}
	for (n = 0; n < count; n++)
		if (controls[n])
			pci_write_config_word(devices[n], PCI_COMMAND,
				commands[n] & ~PCI_COMMAND_MEMORY);
	/* Release only 64-bit prefetchable resources, children first. */
	for (n = count - 1; n >= -1; n--) {
		struct pci_dev *dev = n < 0 ? root : devices[n];
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];
			if (!(r->flags & IORESOURCE_PREFETCH) ||
			    !(r->flags & IORESOURCE_MEM_64) || !r->parent)
				continue;
			ret = pci_release_resource(dev, i);
			if (ret)
				goto out;
		}
	}
	for (n = 0; n < count; n++) {
		u32 value, actual;
		if (!controls[n])
			continue;
		pci_read_config_dword(devices[n], controls[n], &value);
		value = (value & ~PCI_REBAR_CTRL_BAR_SIZE) | (16 << PCI_REBAR_CTRL_BAR_SHIFT);
		pci_write_config_dword(devices[n], controls[n], value);
		pci_read_config_dword(devices[n], controls[n], &actual);
		if (actual != value) {
			ret = -EIO;
			goto out;
		}
	}
	/* Four 128 GiB GPU windows plus one 64 GiB gap for the empty port. */
	window(root, 9 * SZ_64G);
	ret = pci_assign_resource(root, PCI_BRIDGE_PREF_MEM_WINDOW);
	if (ret)
		goto out;
	for (n = 0; n < count; n++) {
		struct pci_dev *dev = devices[n];
		if (!dev->subordinate)
			continue;
		window(dev, dev->bus == root->subordinate ? 9 * SZ_64G : 2 * SZ_64G);
		if (list_empty(&dev->subordinate->devices))
			window(dev, SZ_2M);
		ret = pci_assign_resource(dev, PCI_BRIDGE_PREF_MEM_WINDOW);
		if (ret)
			goto out;
	}
	for (n = 0; n < count; n++) {
		struct pci_dev *dev = devices[n];
		if (!controls[n])
			continue;
		for (i = 1; i <= 3; i += 2) {
			struct resource *r = &dev->resource[i];
			r->start = 0;
			r->end = (i == 1 ? SZ_64G : SZ_32M) - 1;
			r->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH | IORESOURCE_MEM_64 |
				IORESOURCE_SIZEALIGN | IORESOURCE_UNSET | 0xc;
			ret = pci_assign_resource(dev, i);
			if (ret)
				goto out;
		}
	}
	program_window(root);
	for (n = 0; n < count; n++)
		if (devices[n]->subordinate)
			program_window(devices[n]);
	for (n = 0; n < count; n++) {
		if (!controls[n])
			continue;
		pci_write_config_word(devices[n], PCI_COMMAND, commands[n]);
		dev_info(&devices[n]->dev, "BAR1 layout: 64 GiB assigned %pR; BAR3 %pR\n",
			 &devices[n]->resource[1], &devices[n]->resource[3]);
	}
out:
	if (ret)
		pr_err("bar1_layout: root %02x failed (%d); do not bind NVIDIA\n", root_bus, ret);
	pci_unlock_rescan_remove();
	pci_dev_put(root);
	return ret;
}
static void __exit layout_exit(void) { }
module_init(layout_init);
module_exit(layout_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("server-161 driverless four-GPU 64 GiB BAR1 resource layout");
