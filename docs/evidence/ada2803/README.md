# RTX 4060 Ti 16GB 10de:2803 stock evidence

One local RTX 4060 Ti 16GB (`10de:2803`, AD106, 34 SMs) was measured on
2026-10-25 UTC in its stock speed-select state. The host used Linux
`7.2.11-arch1-1`, NVIDIA `615.78.08`, CUDA `13.4.92` and the existing 165 W
power cap. Clock and power policies were retained. Preflight reported 0% GPU
utilization and 42 MiB of GPU memory in use; a desktop compositor was present.

This is **one stock-only measurement**, with five internal timing repetitions
per instruction mode. No same-card unlocked throughput measurement was made.
The separate [2805 evidence](../ada2805/README.md) concerns a different card
and is not a matched comparison for this baseline.

## Instruction throughput

`stock-mma.txt` is unchanged stdout from
[`mma_bench.cu`](../../../diagnostics/bench/mma_bench.cu), compiled with
`nvcc -O3 -arch=sm_89 -std=c++17` and run as `mma_bench 20000 5`. The fixed
grid used 1,056 blocks and eight warps per block. Operands and accumulators
remain in registers inside the timed loop; a warm-up launch precedes each
CUDA-event-timed launch. The table reports the best of five repetitions;
the raw file retains all repetitions.

| Instruction mode | Stock best rate |
|---|---:|
| FP16 / FP16 accumulate | 93.1 TF/s |
| FP16 / FP32 accumulate | 46.4 TF/s |
| BF16 / FP32 accumulate | 46.4 TF/s |
| INT8 / INT32 accumulate | 185.4 TOP/s |
| `dp4a` | 46.2 TOP/s |
| scalar FP32 `ffma` | 22.2 TF/s |

The raw program's `TF/s` label also covers integer operations; this table
labels them `TOP/s`. Counting uses two operations per multiply-accumulate
and no sparsity multiplier. These are instruction issue rates, not measured
application GEMM, inference or training throughput.

## State and numerical checks

`sm-rates-stock.json` and `sm-rates-stock-after.json` are stdout from the
read-only [`tools/read_sm_rates.py`](../../../tools/read_sm_rates.py), before
and after the suite. FMLA32, IMLA2 and IMLA4 remained `1` (half rate); the
other six fields remained `0` (full rate). No `gpu_unlock.*` kernel options
were present.

`stock-numeric.txt` contains stdout from
[`validate_gemm.cu`](../../../diagnostics/bench/validate_gemm.cu). All six
seeded FP16/BF16 cuBLAS-versus-CPU checks passed with zero mismatches and
`max_abs=0`, at shapes 256×256×256, 64×96×128 and 127×65×95. These checks
exercise particular arithmetic paths and do not establish general numerical
accuracy. The instruction probe itself does not compare outputs to a reference.

## Files, telemetry and limits

- `stock-gpu-before.csv` and `stock-gpu-after.csv` retain the GPU identity,
  driver, power cap, utilization and memory queries around the suite.
- `stock-telemetry.csv` retains all 80 samples collected at approximately
  100 ms intervals across numerical validation and the instruction benchmark.
  Timestamps are local Asia/Singapore time (UTC+08:00). Among the 68 samples
  reporting at least 50% GPU utilization, SM clocks were 2730–2760 MHz,
  temperatures were 48–66 °C and observed power was 31.02–74.42 W.
- `stock-kernel-gpu-errors.txt` is the empty stdout from the post-suite
  current-boot journal query for `NVRM: Xid` or `fallen off`. Stderr was also
  empty; the original collection did not retain that command's exit status.
- `nvcc-version.txt` preserves compiler version output. `metadata.json`
  records stage times, commands, configuration, source and binary hashes,
  sanitization and limitations. All four benchmark source files match the
  recorded commit; other project files had local modifications. Binaries
  are not included.
- `summary.json` is derived from the preserved results and telemetry.

GPU UUIDs are replaced with `GPU-UUID-OMITTED`, actual PCI locations with
`<target>`, and build paths in metadata with `bin/`. Numeric results and
telemetry are unchanged. Short samples do not establish maximum board power,
long-term thermals or stability. Background desktop activity and clock ramp
can influence timings. There is no measured unlock speedup in this dataset.

For a future matched comparison, follow the
[stock/unlocked collection workflow](../../testing.md#collect-stock-first-then-unlocked)
using the same card, kernel, driver, binaries, launch settings, clock policy
and power cap. `SHA256SUMS` covers every other file in this directory,
including this README. Verify from this directory with:

```sh
sha256sum -c SHA256SUMS
```
