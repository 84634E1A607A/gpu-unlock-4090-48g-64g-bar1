# GPU Unlock: quick start

This guide is for a Linux machine you control. The RTX 4070 Ti SUPER path is
experimental and has been tested on **one** `10de:2705` card. RTX 4090
`10de:2684` has a separate [single-card report](rtx4090-register-report.md).
RTX 4060 Ti 16GB `10de:2805` is experimental and has been unlocked on **one**
card ([2805 report](docs/ada-2805-report.md)). RTX 4050 Mobile `10de:28e1`
is experimental and has been unlocked on **one** board ([28e1 report](docs/ada-28e1-report.md)).
RTX 4060 Mobile `10de:28e0` is also an experimental AD107 target enabled under the same flag.
RTX 4070 non-Ti `10de:2786` is experimental and validated on one Gigabyte card
on native Linux ([2786 report](docs/ada-2786-report.md)).
RTX 4070 Ti `10de:2782` is experimental and has been unlocked on **one** card
([2782 report](docs/ada-2782-report.md)).
The AD102 RTX 4070 Ti SUPER `10de:2689` is experimental and has been unlocked on one card.
The RTX 4070 built on AD103 (`10de:2709`) shares the 2705 path and has been unlocked on **one** card ([2709 report](docs/ada-2709-report.md)).
RTX 4060 Laptop GPU `10de:28a0` is an experimental AD107 target unlocked on **one** card ([28a0 report](docs/ada-28a0-report.md)).
RTX 4060 Ti `10de:2803` shares the experimental AD106 path and has been unlocked on **one** 16GB card ([2803 report](docs/ada-2803-report.md)).
Other Ada IDs remain disabled except for the listed experiments.
The measured instruction-rate gain is for
FP16, BF16 and TF32 tensor work with FP32 accumulation. INT8 and unsigned
INT4 did not speed up in the 2705 tests. One matched FP8/FP32 GEMM was
effectively unchanged (59.326 → 59.495 TF/s).

RTX 4080 `10de:2704` is an experimental target tested on one modified-32-GiB
card; the [hardware report](docs/rtx4080-hardware-report.md) includes a matched
Qwen prefill comparison. Other cards, memory configurations, boot repeatability,
and gains on other workloads remain unverified.

## 1. Find your GPU's PCI address

```sh
lspci -Dnn | grep -i nvidia
```

Find the line for your GPU. The full address is the first field, such as
`0000:01:00.0`. Confirm the device ID is `10de:2705` for the experimental
RTX 4070 Ti SUPER path, `10de:2805` for the experimental RTX 4060 Ti 16GB path,
`10de:2704` for the experimental RTX 4080 path,
`10de:28e1` (RTX 4050 Mobile) or `10de:28e0` (RTX 4060 Mobile) for the experimental AD107 path,
`10de:2786` for the experimental RTX 4070 non-Ti path,
`10de:2782` for the experimental RTX 4070 Ti path,
`10de:2689` for the experimental AD102 RTX 4070 Ti SUPER path,
`10de:2709` for the experimental AD103 RTX 4070 path,
`10de:28a0` (RTX 4060 Laptop GPU) for the experimental AD107 path,
`10de:2803` for the experimental RTX 4060 Ti AD106 path,
or `10de:2684` for the RTX 4090 path. Keep this address
for step 3. On a multi-GPU host, identify the exact intended card; do not use
`targets=all` for an Ada test.

## 2. Install from open source

Install Git, your running kernel's headers, a C build toolchain, DKMS, Python 3,
and `zstd` using your distribution's package manager. Then:

```sh
git clone https://github.com/JustAResearcher/gpu-unlock-ada.git
cd gpu-unlock-ada
make install
```

`make install` selects the Arch, Debian/Ubuntu, or Fedora/RHEL package path.
The build downloads **SHA-pinned NVIDIA 610.43.03 inputs** and generates the
firmware locally. The downloadable source contains no NVIDIA firmware binary.
The build may take several minutes and asks for `sudo` when installing.

If your distribution cannot resolve a build dependency automatically, use
`make install-arch`, `make install-deb`, or `make install-rpm` as appropriate;
the [README](README.md#build-and-install) explains the generated package paths.

## 3. Enable one exact card

From the cloned folder, run this read-only helper to print the exact option
for each eligible GPU it finds:

```sh
python3 tools/print_boot_option.py
```

Choose **one** printed line for the first cold test. The examples below show
the format if you prefer to enter it manually.

For an RTX 4070 Ti SUPER `10de:2705` or an AD103 RTX 4070 `10de:2709`, append these **two** options to your
kernel command line, replacing the sample address with yours:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad103_experimental=1
```

For an RTX 4070 non-Ti `10de:2786`, use the same two options above. This
shares the existing AD103 opt-in as tested, but admits only the exact 2786
AD104 PCI ID; it does not enable other AD104 cards.

For an RTX 4060 Ti `10de:2803` or `10de:2805`, use its own experimental flag
instead:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad106_experimental=1
```

For an RTX 4080 `10de:2704`, use its own experimental flag and one exact PCI
address:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.rtx4080_experimental=1
```

For an RTX 4050 Mobile `10de:28e1` or RTX 4060 Mobile `10de:28e0`, use their shared experimental flag instead:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad107_experimental=1
```

For an RTX 4070 Ti `10de:2782`, use its own experimental flag:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad104_experimental=1
```

For an AD102 RTX 4070 Ti SUPER `10de:2689` (untested on silicon), use its own
experimental flag:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.rtx4070tis_ad102_experimental=1
```

For an RTX 4060 Laptop GPU `10de:28a0`, use its own experimental flag instead:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad107_experimental=1
```

For an RTX 4090 `10de:2684`, only the first option is needed:

```text
gpu_unlock.targets=0000:01:00.0
```

Use the bootloader method your machine already has:

- **Ubuntu/Debian with GRUB:** Edit `GRUB_CMDLINE_LINUX_DEFAULT` in
  `/etc/default/grub`, append the options *inside its existing quotes*, save,
  then run `sudo update-grub`.
- **Arch with GRUB:** Make the same edit, then run
  `sudo grub-mkconfig -o /boot/grub/grub.cfg`.
- **Fedora/RHEL with GRUB:** Run
  `sudo grubby --update-kernel=ALL --args="gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad103_experimental=1"`,
  substituting your PCI address and omitting the experimental flag for 4090.
- **systemd-boot:** Run `bootctl status` to find the active entry. Add the
  options to that entry's `options` line. Make sure its `initrd` points to the
  newly generated initramfs containing `gpu_unlock`; some distributions copy
  `/boot/initrd.img-*` to the EFI partition separately. Keep a copy of the
  original entry so you can select it if needed.

**Keep a normal boot entry available.** Do not copy a sample PCI address.
The package does nothing until these options are present.

## 4. Start from full power-off

Shut down cleanly. Cut AC power to the machine, wait about 30 seconds, then
restore power and start it. This gives the first test a fresh card state.
If the GPU has a customer or rented workload, wait until it is fully released
before doing this.

## 5. Check the result

After the stock NVIDIA driver loads, run:

```sh
journalctl -k -b --no-pager | grep -E 'gpu_unlock: unlocked|gpu_unlock: unlock failed|post-FLR SEC2'
nvidia-smi
```

Success requires the **exact selected card's** `gpu_unlock: unlocked` line,
healthy `nvidia-smi`, and a workload check. A loaded module alone is not
proof. The [2705 report](docs/ada-2705-report.md) shows the expected marker,
permission/selector readbacks, matched performance and numerical checks from
the tested card. For a single-GPU host, the read-only rate getter is:

```sh
python3 tools/read_sm_rates.py --expect-uuid GPU-PASTE-YOUR-EXACT-UUID
```

Get the UUID from `nvidia-smi --query-gpu=uuid --format=csv,noheader` after
boot. Do not use a UUID from another card.

For a stock/unlocked comparison, collect the stock measurements before enabling
the test and use the same running kernel, driver, binaries and power cap in both
states. Check `uname -r` and `/proc/cmdline`; a boot-menu title alone does not
identify the kernel or prove that the test entry ran. The [testing guide](docs/testing.md)
includes benchmark commands and the [matched RTX 4090 results](docs/evidence/rtx4090-pr1-matched/README.md).

## Keep it enabled across boots

First test an ordinary reboot with the same early initramfs and exact target
options selected for **one boot**. Check for `gpu_unlock: unlocked`, healthy
`nvidia-smi`, and full rate readbacks again. On the tested 4070 Ti SUPER, a
warm reboot returned the card to a fresh enough state and the unlock
succeeded; other boards may retain firmware state and be rejected. A loaded
module alone does not prove the unlock succeeded.

After that test succeeds, leave the target options in your persistent GRUB
entry. With systemd-boot, make the *tested* Ada entry persistent using
`sudo bootctl set-default YOUR-ADA-ENTRY.conf`, replacing the entry ID with
the one shown by `bootctl status`. Keep a separate stock entry in the boot
menu. `bootctl set-oneshot` selects only the next boot and will not persist.
After a kernel update, verify that the selected entry points to a rebuilt
initramfs containing `gpu_unlock` before booting the new kernel.

## Roll back

Remove the `gpu_unlock.*` kernel-command-line options and boot the normal
entry. If the firmware transaction did not stop cleanly, power off completely
before retrying; do not force-unload the module while it may retain DMA buffers.
Uninstall the generated packages if you no longer want the module or firmware.
If you used `bootctl set-default` for a separate Ada entry, run
`sudo bootctl set-default YOUR-STOCK-ENTRY.conf` and confirm the selected
default with `bootctl status` before rebooting.

For licensing, provenance and distribution limits, see
[firmware/NOTICE.md](firmware/NOTICE.md) and [the release notes](docs/release-1.1.1.md).
