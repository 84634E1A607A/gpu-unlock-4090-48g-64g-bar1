# Stock RTX 4090 baseline for PR #1

This is the archived LTS stock-only run. A later [matched stock/unlocked
comparison](../rtx4090-pr1-matched/README.md) uses regular Linux `7.2.11-arch1-1`
for both states. Do not compare these LTS timings against that unlocked run.

These measurements were collected on one owner-controlled RTX 4090 on
2026-10-24 UTC, using the tools at commit `8855e7b`, NVIDIA `615.78.08`,
Linux `6.18.57-2-lts`, CUDA `13.4.92`, and the existing 450 W power cap.
The public RM getter reported half-rate FMLA32 (`fmla32=1`). No unlock module
was loaded. These numbers are a baseline, not a measured unlock speedup or
a hardware validation of the changed kernel module.

| Instruction mode | Stock best rate, five repetitions |
|---|---:|
| FP16 / FP32 accumulate | 159.5 TF/s |
| BF16 / FP32 accumulate | 159.4 TF/s |
| FP16 / FP16 accumulate | 318.9 TF/s |
| INT8 / INT32 accumulate | 637.5 TOP/s |
| `dp4a` | 158.5 TOP/s |
| scalar FP32 `ffma` | 76.1 TF/s |

The instruction benchmark used 20,000 iterations, 1,056 blocks, eight warps
per block and the same source launch settings as `diagnostics/bench/mma_bench.cu`.
Its original stdout is in `mma.txt`. `tensor-modes.json` contains the separate
FP16/TF32/unsigned-INT4 probe, with five repetitions and every output cell
checked. Its JSON was formatted and the identifying GPU string sanitized;
timings, rates, launch geometry and mismatch counts are preserved.

Real cuBLAS dense GEMMs, with FP32 accumulation, measured:

| Square GEMM | FP16 median | BF16 median |
|---|---:|---:|
| 1024³ | 108.58 TF/s | 126.33 TF/s |
| 4096³ | 161.18 TF/s | 153.48 TF/s |
| 8192³ | 164.94 TF/s | 163.44 TF/s |

`gemm.jsonl` preserves all timing repetitions and verification counts. The
standalone probe's exact source is included as `gemm_bench.cu`. It uses ten
warmups, twenty GEMMs per timing, five timings per shape/type, and checks every
output cell against an exact reference. Its performance inputs intentionally
have a cheap reference; they do not establish general numerical correctness.

The changed `diagnostics/bench/validate_gemm.cu` separately passed FP16 and
BF16 checks for all three seeded shapes, with zero mismatches and zero maximum
absolute error. `numerical.txt` preserves that output except for the PCI address.

The GNOME desktop remained active. This session did not change clocks or power
limits. Active telemetry ranged from 2,535 to 2,655 MHz SM clock and 61–76 °C;
the largest observed sampled power was 303.82 W. Clock, temperature and desktop
variation can affect timing. This archived run has no matched unlocked
measurement; use the linked regular-kernel report for the paired comparison.

`metadata.json` records the settings, rate getter values, telemetry summary and
source/binary hashes. GPU UUID, PCI location, host paths and boot ID are omitted.
The standalone GEMM probe can be rebuilt without installing anything:

```sh
nvcc -O3 -arch=sm_89 -std=c++17 -o /tmp/gemm_bench \
  docs/evidence/rtx4090-pr1-stock/gemm_bench.cu -lcublas
```

To check the included evidence files from this directory:

```sh
sha256sum -c SHA256SUMS
```
