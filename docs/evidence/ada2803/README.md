# RTX 4060 Ti 16GB 10de:2803 evidence

One physical AD106 card reporting `10de:2803` and 16,380 MiB was tested on
October 25, 2026. The [2803 report](../../ada-2803-report.md) describes the
successful unlock, stock-driver handoff, throughput and limits.

The saved stock measurements used Linux `7.2.11-arch1-1`; the unlocked run
used `6.18.58-1-lts`. Both used NVIDIA `615.78.08`, CUDA `13.4.92`, the same
165 W cap and identical benchmark binaries. **The kernel difference means
this is not a fully matched stock/unlocked comparison.** No clock or power
policy was changed. A desktop compositor was present.

| Probe | Saved stock | Unlocked |
|---|---:|---:|
| MMA FP16 / FP32 | 46.4 TF/s | 92.7 TF/s |
| MMA BF16 / FP32 | 46.4 TF/s | 92.7 TF/s |
| Tensor-mode TF32 / FP32 | 23.184 TF/s | 46.173 TF/s |
| Tensor-mode unsigned INT4 / INT32 | 368.801 TOP/s | 368.638 TOP/s |

All six seeded cuBLAS-versus-CPU numerical cases passed in each state,
with zero mismatches and `max_abs=0`. Each tensor-mode run checked all
270,336 output cells per mode with zero mismatches. The RM rate getter
reported FMLA32/IMLA2/IMLA4 `1` (half) stock and `0` (full) unlocked.

## Files

- `transaction-excerpt.txt`: selected card's unlock messages from the
  observed boot; no complete system journal or boot/root identifiers.
- `sm-rates-stock*.json`, `sm-rates-unlocked*.json`: read-only RM getter
  output around the instruction suites.
- `stock-mma.txt`, `unlocked-mma.txt`: raw `mma_bench 20000 5` output,
  including the unchanged integer and scalar controls.
- `stock-numeric.txt`, `unlocked-numeric.txt`: seeded numerical checks.
- `tensor-modes-stock.json`, `tensor-modes-unlocked.json`: raw tensor-mode
  runner output, 20,000 iterations and three repetitions. Both use cubin
  SHA-256 `33aa6db385aa9eb8a1f73bb2f74891aea84794326b7c40992bc642f93a3c614d`,
  also used in the separate [2805 evidence](../ada2805/README.md).
- `metadata.json`, `tensor-modes-stock-metadata.json`,
  `unlocked-metadata.json`: commands, hashes, kernels, timing and limitations.
- `stock-gpu-*.csv`, `unlocked-gpu-*.csv` and `*-telemetry.csv`: identity,
  cap and approximately 100 ms clock/temperature/power samples. Telemetry
  timestamps use Asia/Singapore (UTC+08:00); metadata timestamps use UTC.
- `*-kernel-gpu-errors.txt`: empty scans for GPU errors. The unlocked scan
  was taken after all workloads in the observed boot.
- `nvcc-version.txt`: toolkit version; `summary.json`: original top-level
  stock results plus the new `unlocked` results and comparison limits.

GPU UUIDs are replaced with `GPU-UUID-OMITTED`, actual PCI locations with
`<target>`, and private build paths with `bin/`. Numeric results and timing
repetitions are retained. No firmware or benchmark binary is included.

One firmware transaction was observed. The owner reported a reboot; an
AC-cold start and a second reboot were not independently tested. These
short instruction and correctness runs do not establish application speedup
or long-term stability. The 2805 uses a different card and is not this
baseline.

`SHA256SUMS` covers every other file in this directory. Verify with:

```sh
sha256sum -c SHA256SUMS
```
