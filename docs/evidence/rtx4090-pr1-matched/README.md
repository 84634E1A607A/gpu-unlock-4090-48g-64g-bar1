# Matched RTX 4090 validation of PR #1

An independent owner-controlled RTX 4090 (`10de:2684`) completed the reviewed
module's transaction and stock NVIDIA driver handoff on 2026-10-24 UTC.
The executed module source was revision `8855e7bca8ff142204e02301331fd56b26699819`.
The published evidence commit does not change the module or firmware payload.

Both measurements used Linux `7.2.11-arch1-1`, NVIDIA `615.78.08`, CUDA
`13.4.92`, the same exact GPU and benchmark binaries, and the existing 450 W
power cap. The GNOME desktop remained active. Clocks and power policy were
not changed. This is **one matched stock/unlocked pair**, with five internal
timing repetitions per mode. The separate [LTS stock-only run](../rtx4090-pr1-stock/README.md)
is archived context and is not used in these ratios.

## Boot and correctness evidence

The test used a separate regular-kernel initramfs containing the reviewed
module and all nine pinned firmware assets, loaded before NVIDIA. The extracted
image was checked for exact asset/module bytes, matching kernel/NVIDIA module
versions and early module load order. Module and firmware hashes are in
`metadata.json`; no firmware binary is included here.

The selected card reported `gpu_unlock: unlocked` before NVIDIA initialization
([excerpt](unlocked-boot-excerpt.txt)). After the driver initialized, the public
RM getter changed from `fmla32=1` (half rate) to `fmla32=0` (full rate); both
sanitized readbacks are included. No `NVRM: Xid` errors were observed before
or after the benchmark suite; the later journal snapshot is recorded in
`post-benchmark-kernel-check.json`.

Both states passed all six seeded FP16/BF16 numerical cases, with zero
mismatches and `max_abs=0`: 256×256×256, 64×96×128, and 127×65×95.
Every output cell in each large cuBLAS GEMM and each tensor-mode control also
passed its exact-reference check. These are exercised-path checks, not proof
of arbitrary application accuracy.

## Instruction throughput

`mma_bench` used 20,000 iterations, 1,056 blocks, eight warps per block and the
best of five timing repetitions. Every reported repetition is retained in
`stock-mma.txt` and `unlocked-mma.txt`.

| Mode | Stock | Unlocked | Ratio |
|---|---:|---:|---:|
| FP16 / FP32 accumulate | 160.4 TF/s | 320.4 TF/s | 1.998× |
| BF16 / FP32 accumulate | 159.7 TF/s | 320.2 TF/s | 2.005× |
| FP16 / FP16 accumulate | 320.6 TF/s | 320.4 TF/s | 0.999× |
| INT8 / INT32 accumulate | 641.1 TOP/s | 640.3 TOP/s | 0.999× |
| `dp4a` | 159.1 TOP/s | 159.1 TOP/s | 1.000× |
| scalar FP32 `ffma` | 76.4 TF/s | 76.3 TF/s | 0.999× |

The separate same-cubin tensor-mode probe also measured TF32/FP32
**80.138 → 159.453 TF/s** (1.990×). Its FP16/FP32 control was
160.253 → 318.943 TF/s, and unsigned INT4/INT32 was essentially unchanged
at 1,273.218 → 1,273.675 TOP/s. Those repetitions and zero-mismatch counts
are retained in the two `*-tensor-modes.json` files. These are instruction
throughput measurements, not application speedups.

## cuBLAS GEMM throughput

Square dense GEMMs with FP32 accumulation used ten warmups, twenty GEMMs per
timing, and five timing repetitions. The table reports median throughput.

| N | Input type | Stock TF/s | Unlocked TF/s | Ratio |
|---|---|---:|---:|---:|
| 1024 | FP16 | 107.546 | 127.948 | 1.190× |
| 1024 | BF16 | 127.100 | 151.968 | 1.196× |
| 4096 | FP16 | 153.234 | 205.956 | 1.344× |
| 4096 | BF16 | 153.418 | 152.720 | 0.995× |
| 8192 | FP16 | 168.675 | 188.891 | 1.120× |
| 8192 | BF16 | 166.197 | 192.744 | 1.160× |

The unchanged 4096³ BF16 result is retained. `comparison.json` contains the
unrounded ratios; the two `*-gemm.jsonl` files preserve every timing and output
verification count. The exact [standalone probe source](../rtx4090-pr1-stock/gemm_bench.cu)
is unchanged from the archived run; its source and binary hashes match the
new metadata. The performance matrices have a cheap exact reference and are
not representative of general numerical inputs. Seeded numerical validation
was run separately in each state.

## Telemetry and limits

Among samples with at least 50% reported GPU utilization:

| Sampled measurement | Stock | Unlocked |
|---|---:|---:|
| SM clock range | 2655–2670 MHz | 2610–2670 MHz |
| GPU temperature range | 50–68 °C | 54–73 °C |
| Largest observed power sample | 307.67 W | 308.59 W |

The two telemetry CSVs retain all samples with the UUID column removed.
Their timestamps are local Australia/Victoria time (UTC+11). Sampling was
approximately every 100 ms; these short samples do not establish maximum
board power, long-term thermals or electrical safety.

Desktop activity, temperature and clock variation can influence timing.
There was no extended stability test or representative inference/training
workload in this run. A later ordinary warm reboot and unlocked rerun are
documented separately in [warm-reboot-repeat](warm-reboot-repeat/README.md).
That follow-up is one unlocked repeat against this same single stock baseline,
not an additional matched pair. Physical AC removal was not independently
observed. These results validate only the exercised reviewed module path on
this card/configuration; they do not validate every supported board or kernel.

## Warm-reboot repeat

After an ordinary warm reboot, the isolated test image again reported
`gpu_unlock: unlocked` before NVIDIA initialization. The post-driver selector
readback remained full rate, and the second unlocked benchmark suite passed
with zero seeded or GEMM output mismatches and no observed Xid errors. The
FP16/FP32 and BF16/FP32 issue-rate probes remained near 2× versus the original
stock run. Application GEMM results varied by shape: five of six medians were
faster, while 4096³ BF16 was 8.2% slower. Full results, sanitized logs and
checksums are in [warm-reboot-repeat](warm-reboot-repeat/README.md).

## Reproduce and verify

The [testing guide](../../testing.md) separates host tests, boot success and
workload checks, and includes the same workload build/collection commands.
Kernel and driver must match across the two measurements; a boot-menu title
alone is insufficient. The local test entry included a text-console `COLD`
confirmation; the normal Quick Start does not install that prompt.

GPU UUID, actual PCI location, host name, boot/root identifiers and private
paths are omitted. MMA/GEMM stdout is unchanged; numerical stdout substitutes
`<target>` for the PCI address. JSON formatting/identity fields and telemetry
UUID removal are documented in `metadata.json`; numeric results are preserved.
The boot log is a selected sanitized excerpt, not a complete log archive.

From this directory, check every included evidence file with:

```sh
sha256sum -c SHA256SUMS
```
