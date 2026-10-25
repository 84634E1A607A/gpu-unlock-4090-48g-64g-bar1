# RTX 4060 Ti 16GB (10de:2803) Ada canary

On October 25, 2026, one AD106 RTX 4060 Ti reporting `10de:2803` and
16,380 MiB of VRAM completed the existing AD102-family unlock path. The
unmodified NVIDIA `615.78.08` driver initialized, the read-only RM getter
reported full rate, and the exercised CUDA correctness checks passed.
This validates one card and one observed firmware transaction. PCI ID alone
does not determine VRAM capacity.

The tested boot ran Arch Linux `6.18.58-1-lts` with a single exact PCI
target and `gpu_unlock.ad106_experimental=1`. The owner reported a reboot;
an AC-cold start was not independently confirmed. The module's entry
readback showed WPR2 high `0x0`, allowing its fresh-state checks to pass.
No second firmware transaction or reboot repeat was performed during this
collection. See the [sanitized raw evidence](evidence/ada2803/README.md).

## Transaction and driver handoff

The module used the pinned NVIDIA `610.43.03` Booter/GSP assets and the
unchanged four-PLM payload. The boot log recorded:

- chip `0x196000a1`, entry SS0 `0x15274054`, SS1 `0x0`, WPR2 high `0x0`;
- payload marker `0x67b00001`, required permissions accepted, SEC2 halted;
- WPR_CFG `0xfffff0ff`, WPR `0xffffffff` and FEAT `0xffffffff` after the
  payload; the observational FBPA value remained `0xffffff0f`;
- selectors SS0 `0x88888888`, SS1 `0x8`, with all nine effective readout
  fields zero both before and after FLR;
- post-FLR SEC2 reset released, GSP halted, bus mastering disabled and WPR2
  high zero, followed by `gpu_unlock: unlocked`.

FMLA32, IMLA2 and IMLA4 changed from `1` (half) in the saved stock evidence
to `0` (full). All nine RM rate fields were zero before and after the new
workload checks. The current boot journal contained no NVRM Xid,
GPU-fallen-off or GSP failure/timeout messages after those checks.

## Throughput and correctness

The saved stock run used Linux-LTS kernel `6.18.58-1-lts`. Both used the same physical GPU, NVIDIA `615.78.08`, CUDA `13.4.92`, the existing 165 W cap and the exact same benchmark binaries.

`mma_bench 20000 5` measured these best register-resident instruction rates;
all timing repetitions are retained in the raw files:

| Mode | Saved stock | Unlocked |
|---|---:|---:|
| FP16 / FP16 accumulate | 93.1 TF/s | 92.7 TF/s |
| FP16 / FP32 accumulate | 46.4 TF/s | 92.7 TF/s |
| BF16 / FP32 accumulate | 46.4 TF/s | 92.7 TF/s |
| INT8 / INT32 accumulate | 185.4 TOP/s | 185.4 TOP/s |
| `dp4a` | 46.2 TOP/s | 46.2 TOP/s |
| Scalar FP32 `ffma` | 22.2 TF/s | 22.2 TF/s |

The hash-identical tensor-mode cubin used for the saved stock run and the
separate `2805` evidence was also run with 20,000 iterations and three
repetitions. It checked all 270,336 output cells per mode:

| Mode | Saved stock | Unlocked | Ratio | Mismatches |
|---|---:|---:|---:|---:|
| FP16 / FP32 accumulate | 46.621 TF/s | 92.334 TF/s | 1.981× | 0 |
| TF32 / FP32 accumulate | 23.184 TF/s | 46.173 TF/s | 1.992× | 0 |
| Unsigned INT4 / INT32 | 368.801 TOP/s | 368.638 TOP/s | 1.000× | 0 |

The separate cuBLAS-versus-CPU validation passed all six seeded FP16/BF16
cases, including rectangular and non-tile-aligned shapes, with zero
mismatches and `max_abs=0`. These checks validate the exercised paths;
they do not establish arbitrary numerical accuracy.

The approximately seven-second suite retained 100 ms telemetry samples.
Among samples with at least 50% GPU utilization, SM clocks ranged from
2,730 to 2,745 MHz, temperature from 45 to 63 °C, and sampled board power
from 40.20 to 82.88 W. These short samples do not establish peak power,
thermal limits or extended stability.

## Limits and follow-up

These are instruction issue rates and small numerical checks, not measured
application GEMM, inference or training speedups. A comparison with the same
kernel in both states, an independently confirmed cold start, repeated
boots, longer workloads and other `2803` boards remain to be tested.
The existing `2805` result concerns another card and is not the stock
baseline for this report.
