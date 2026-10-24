# RTX 4080 experimental hardware report

## Setup

- GPU: NVIDIA GeForce RTX 4080, PCI ID `10de:2704`.
- VRAM: modified 32 GiB configuration; the NVIDIA driver reported 32,760 MiB.
- Guest OS: Ubuntu 24.04, Linux `6.8.0-147-generic`; NVIDIA driver `580.178.04`.
- The GPU was passed through to a virtual machine.
- Application workload: public Qwen 27B EXL3 6.0 bpw checkpoint with ExLlamaV3 `1.6.2`; PyTorch `2.11.0+cu130`.

## Unlock and hardware readbacks

A guarded trial with the experimental option and one exact PCI target completed successfully on this single card. The kernel path reported `gpu_unlock: unlocked`; the firmware marker matched the expected value, required permissions were present, and the boot processor reached the expected halted state. The effective hardware readout reported full rates before and after function-level reset.

An independent NVIDIA resource-manager getter read showed the FMLA32, IMLA2, and IMLA4 encodings change from `1` to `0`. The driver enumerated 32,760 MiB of VRAM after the trial.

## CUDA numerical check and synthetic GEMM

FP16 and BF16 CUDA GEMMs (2048 x 2048, FP32 accumulation) produced bitwise-identical before/after outputs by full SHA-256. Relative Frobenius errors against FP64 were unchanged: `0.00035876` for FP16 and `0.00286997` for BF16. Median time changed from `0.21929 ms` to `0.11544 ms` for FP16 and from `0.21934 ms` to `0.11546 ms` for BF16, at the same 320 W power limit with clocks unlocked. This synthetic GEMM result is distinct from the application measurements below.

## Native server prompt A/B

| Prompt tokens | Median tokens/s before | Median tokens/s after | Change | Median prompt time before | Median prompt time after |
|---:|---:|---:|---:|---:|---:|
| 4,096 | 1,785.59 | 2,140.84 | +19.90% | 2.29 s | 1.91 s |
| 16,384 | 1,629.55 | 2,002.08 | +22.86% | 10.05 s | 8.18 s |
| 65,536 | 1,284.38 | 1,630.48 | +26.95% | 51.02 s | 40.19 s |

These are native server prompt timings for one stock/unlocked boot pair using the same configuration and prompts. Each size had one warmup and three measured repeats. Prompt SHA-256 values matched before and after; `cached_tokens` was zero in all 12 requests per state. The power limit was 320 W; clocks were not locked. The sanitized numeric data is in [matched-results.json](evidence/ada2704/matched-results.json). The model returned to Ready, no Xid errors appeared after load or benchmark, HTTP 200/JSON probes passed through the real consumer and operator probe, the deployment/configuration remained unchanged, and six service nodes were ready.

## Scope and limits

This is one modified-memory RTX 4080 trial. It does not establish compatibility for other RTX 4080 cards or memory configurations. The application A/B covers one boot pair; separate cold-boot and warm-reboot repeatability have not been tested. Results describe this workload and setup and do not guarantee the same gains on other cards or models.