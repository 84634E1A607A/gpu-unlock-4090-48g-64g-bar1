# bar1_layout_rootport — 64 GiB BAR1 layout for root-port topologies

A companion kernel module for the 64 GiB BAR1 unlock, for hosts where **every
GPU sits directly behind its own PCI root port** (no PCIe switch), e.g. a
dual-socket server with one GPU per root port and a large (1 TiB) host
prefetch window per root bus.

This is a second validated topology next to
[`../server-161-bar1-layout`](../server-161-bar1-layout) (4 GPUs behind a
Microchip switch). If your machine has a PCIe switch, use that one instead;
if your machine has no switch, use this one. Read the upstream
[BAR1 report](../../docs/bar1-64g-report.md) first — the capability side of
the unlock lives in the `gpu_unlock` module; this helper only handles the
Linux-side address-space allocation.

## When you need this

After the payload raises the BAR1 capability to 64 GiB, the stock resources
(assigned at PCI enumeration time, before initramfs) are still the old size,
and the naive resize path can fail with `ENOSPC`. This module extends each
GPU's root-port prefetch window (default 128 GiB per port, base unchanged)
and then assigns the GPU a 64 GiB BAR1 plus its 32 MiB BAR3, before the
NVIDIA driver binds.

## Safety properties

- **Dry-run by default**: with no parameters it only reports what it sees
  (per-GPU BAR1, 64 GiB capability, parent port window) and changes nothing.
- **`commit=1` is required** to change anything.
- **Restore-on-failure**: anything released during a failed commit is
  reassigned to the original (stock) resources, so a failed attempt degrades
  to the previous working configuration rather than leaving BARs unassigned.
- It refuses to touch GPUs that are bound to a driver or bus-mastering.

The GPU bus list is a parameter (hex, comma-separated), defaulting to the
validated 8-card host:

```
modprobe bar1_layout_rootport            # dry-run report on the default list
modprobe bar1_layout_rootport buses="c8,d8" commit=1   # apply to two cards
```

## Boot integration (initramfs, init-premount)

The module must run after the `gpu_unlock` payload (64 GiB capability) and
before NVIDIA binds. On an initramfs-tools host:

1. `make && sudo install -m644 bar1_layout_rootport.ko /lib/modules/$(uname -r)/updates/ && sudo depmod -a`
2. Have your initramfs hook include it (e.g. `manual_add_modules bar1_layout_rootport`).
3. From an init-premount script: check that `dmesg` shows one
   `gpu_unlock: unlocked` marker per target GPU, then
   `modprobe bar1_layout_rootport buses="<your hex bus list>" commit=1`,
   then verify each GPU's `resource` line 2 covers 64 GiB. On failure, just
   continue the boot — the stock 32 GiB BAR1 still works (the driver binds
   normally and nothing is left unassigned).

## Validated setup

- Host: dual Xeon 8570 (4 NUMA), 8x RTX 4090 48G (`10de:2684`), one GPU per
  root port, 1 TiB host prefetch window per root bus.
- Ubuntu 24.04, kernel 6.8.0-149, NVIDIA open driver 610.43.02 with the
  consumer-P2P patch.
- Result: 8/8 GPUs report BAR1 total 65536 MiB after every boot; the P2P
  write bandwidth went from per-pair 27.1 GB/s (32 GiB BAR1, with two slots
  misbehaving) to a uniform 22.7 GB/s across all 56 pairs — see the main
  repo's docs for the measured tradeoff.
