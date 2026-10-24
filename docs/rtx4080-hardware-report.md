# RTX 4080 experimental hardware report

## Setup

- GPU: NVIDIA GeForce RTX 4080, PCI ID `10de:2704`.
- VRAM: modified 32 GiB configuration; the NVIDIA driver reported 32,760 MiB.
- Host software: Ubuntu 24.04 guest, Linux `6.8.0-147-generic`, NVIDIA driver `580.178.04`.
- The GPU was passed through to a Proxmox VFIO virtual machine.

## Observed result

A guarded trial with the experimental option and one exact PCI target completed successfully on this single card. The kernel path reported `gpu_unlock: unlocked`; the firmware marker matched the expected value, required permissions were present, and the boot processor reached the expected halted state. The effective hardware readout reported full rates before and after function-level reset.

An independent read of the NVIDIA resource-manager rate getter showed the measured `fmla32`, `imla2`, and `imla4` modes change from `1` (half rate) to `0` (full rate). The driver enumerated 32,760 MiB of VRAM after the trial.

FP16 and BF16 CUDA GEMMs (2048 x 2048, FP32 accumulation) produced bitwise-identical before/after outputs by full SHA-256. Relative Frobenius errors against FP64 were unchanged: `0.00035876` for FP16 and `0.00286997` for BF16. Across five windows of 20 matmuls, median time changed from `0.21929 ms` to `0.11544 ms` for FP16 and from `0.21934 ms` to `0.11546 ms` for BF16, at the same 320 W power limit with clocks unlocked. This is a synthetic GEMM result, not an application or prefill result.

The source build, real-asset unit tests, and running-kernel module build completed successfully for this setup. Mock tests cover target gating, synthetic 32 GiB metadata, and failure paths; they are not a substitute for hardware evidence.

## Limits

This is one modified-memory RTX 4080 trial. It does not establish compatibility for other RTX 4080 cards or memory configurations. Cold-boot and warm-reboot repeatability have not been demonstrated. A Qwen prefill A/B is still pending; no application-performance claim is made here.
