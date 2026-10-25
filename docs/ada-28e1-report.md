# RTX 4050 Mobile (10de:28e1) Ada canary

On October 25, 2026, the AD102-family four-PLM payload ran on one cold
RTX 4050 Mobile (`10de:28e1`, AD107M, 20 SMs, rev a1) before the unmodified
NVIDIA `615.78.08` open kernel driver. Every gate the module enforces passed,
and the NVIDIA RM getter changed FMLA32, IMLA2 and IMLA4 from half to full rate.
Matched SM89 CUDA tensor benchmarks confirmed an immediate 2.00× issue-rate
speedup for FP16/FP32 accumulation (26.48 → 52.97 TF/s) and TF32/FP32 accumulation (13.21 → 26.42 TF/s).
This is one physical machine (ASUS TUF Gaming A16 FA607NUG) and one cold boot.
It does not establish support for other AD107 boards or desktop AD107 cards.

> [!WARNING]
> **OEM Laptop Architectural Variance Warning:**
> Mobile laptops exhibit massive hardware, firmware, and power routing differences across manufacturers (Lenovo, Dell, HP, Acer, MSI, ASUS). This unlock was verified specifically on the ASUS TUF Gaming A16 FA607NUG and **might not work on other laptops** without pre-flight hardware verification:
> 1. **VBIOS ROM Aperture Exposure:** The unlock requires reading FWSEC-FRTS directly from the VBIOS aperture at BAR0 + `0x300000`. The ASUS FA607NUG exposes a full 1 MiB physical aperture with NPDE/NPDS extensions. Other laptop motherboards or firmware revisions may trap, gate, or truncate this aperture, causing FWSEC extraction to fail.
> 2. **EC and Standby Power Rails:** Resetting volatile fuse state requires a complete Embedded Controller (EC) power discharge (unplugging AC and holding the chassis power button for 40 seconds). Standby power-plane topologies vary across laptop OEMs.
> 3. **PCIe Root Complex & FLR:** Laptop PCIe root ports handle Function Level Reset (FLR) and secondary bus resets differently. Failure to cleanly restore bus mastering or link state during early initramfs boot can lead to boot hangs or kernel panics on untested laptop motherboards.

The [evidence directory](evidence/ada28e1/README.md) contains the selected boot
messages, rate readbacks and matched benchmark outputs with the GPU UUID
replaced.

## Host system specifications

- **Model:** ASUS TUF Gaming A16 (FA607NUG_FA607NUG)
- **BIOS Version:** FA607NUG.306
- **Processor:** AMD Ryzen 7 7445HS w/ Radeon 740M Graphics (8 cores / 16 threads, HawkPoint2)
- **Memory:** 16 GiB DDR5
- **dGPU:** NVIDIA Corporation AD107M [GeForce RTX 4050 Max-Q / Mobile] (`10de:28e1`, rev a1, subsystem `1043:14f4`)
- **GPU Architecture:** AD107 (Chip ID `0x197000a1`, 20 SMs, 6 GiB GDDR6)
- **OS / Distribution:** CachyOS Linux (x86_64)
- **Kernel:** `7.2.10-1-cachyos` (built with Clang 23.1.1)
- **NVIDIA Driver:** Stock NVIDIA open kernel module `615.78.08`
- **Compositor:** Wayland `niri` via AMD iGPU with NVIDIA PRIME offload

The module was built by DKMS and bundled into the initramfs using the mkinitcpio
drop-in (`/etc/mkinitcpio.conf.d/zz-gpu-unlock.conf`).
The boot used a dedicated Limine boot entry with the **single exact PCI target**
and `ad107_experimental=1`:

```text
gpu_unlock.targets=0000:01:00.0 gpu_unlock.ad107_experimental=1
```

The card owner performed a cold start following a full 40-second ASUS Embedded
Controller (EC) hardware reset (unplugged AC and held power button for 40 seconds)
to completely discharge all volatile GPU power rails, and confirmed it was a cold
boot; the module's entry readback reported WPR2 high `0x0`.

## Firmware and driver checks

The module loaded the pinned NVIDIA `610.43.03` stock AD102 Booter/GSP assets
and the generated 64,000-byte payload. The kernel log recorded:

- entry: `SS0=0x54727157`, `SS1=0x3`, WPR2 high `0x0`, chip `0x197000a1`;
- FWSEC: extracted from BAR0 + `0x300000` (length 1 MiB, `0xAA55` header),
  `ucode_id=9`, `engine_mask=0x400`, `sig_versions=0x3` selecting slot 1;
- payload marker `0x67b00001` (expected `0x67b00001`), SEC2 halted;
- the three required permission writes: `WPR_CFG=0xfffff0ff`,
  `WPR=0xffffffff`, `FEAT=0xffffffff`. The observational FBPA field stayed
  `0xffffff0f`, as on the 2705 and 2805;
- selector writes read back `SS0=0x88888888` and `SS1=0x8`, and the
  effective readout changed from fields `0 0 2 0 0 2 0 2 0` to all zero;
- after PCIe FLR the selectors were still `SS0=88888888 SS1=00000008`;
- `gpu_unlock: unlocked`.

The stock NVIDIA `615.78.08` driver then loaded and initialized `nvidia-drm`.
The desktop session (Wayland `niri`) launched and CUDA kernels completed.
The kernel log contained zero NVRM Xid errors or GPU drop-offs.

The read-only RM getter reported:

| Field | Before | After |
| :--- | ---: | ---: |
| FMLA32 | 1 (half) | 0 (full) |
| IMLA2 | 1 (half) | 0 (full) |
| IMLA4 | 1 (half) | 0 (full) |
| IMLA0, FMLA16, DP, FFMA, IMLA1, IMLA3 | 0 | 0 |

The "before" values were read on the same card and driver in an earlier boot,
before this build was installed.

## Matched instruction rate benchmark

A pure CUDA Driver API JIT benchmark measuring SM89 tensor issue rates was run
with three timed repetitions per mode:

| Mode | Stock Baseline | Unlocked | Speedup |
| :--- | ---: | ---: | ---: |
| FP16 → FP32 accumulate | 26.48 TF/s (209.02 ms) | **52.97 TF/s (104.51 ms)** | **2.00×** |
| TF32 → FP32 accumulate | 13.21 TF/s (209.56 ms) | **26.42 TF/s (104.78 ms)** | **2.00×** |

Active telemetry during the unlocked run reported sustained
SM boost clock of **2655 MHz** and temperature of **46°C**.

## Not yet done

- warm-reboot persistence checks across ordinary soft reboots;
- representative cuBLAS / CUTLASS GEMM numerical validation;
- desktop AD107 boards (RTX 4060 desktop) and other mobile variants.
