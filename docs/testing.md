# Testing and interpreting results

Validation has three separate levels. A passing host test does not establish
that a firmware transaction ran, and a successful transaction does not predict
the speedup of an application.

| Level | What it checks | Current evidence |
|---|---|---|
| Host regression tests | Asset pins, rejection/unwind paths, mocked transactions, benchmark helpers | 29 tests passed with pinned inputs; eight C components and full GA102/AD102 cases passed ASan/UBSan |
| Actual boot and driver handoff | Selected-card transaction completed; NVIDIA initialized; rate getter changed | Independent RTX 4090 run of `8855e7b` on Linux `7.2.11-arch1-1`, NVIDIA `615.78.08` |
| Workload checks | Exercised outputs and measured throughput in both states | Six seeded FP16/BF16 numerical cases, six large cuBLAS GEMMs, instruction/control probes; one matched pair |

The [matched RTX 4090 report](evidence/rtx4090-pr1-matched/README.md) contains
the numeric evidence and limitations. Longer stability runs, additional boot
reproductions and representative inference/training workloads remain useful
follow-up tests.

## Host tests without touching a GPU

```sh
make test
```

Missing generated assets/source are explicit skips of the full transaction
tests. For strict validation, generate the pinned inputs and require them:

```sh
make assets CACHE=build/test-inputs
make test-full CACHE=build/test-inputs
```

The first command downloads the pinned NVIDIA inputs and generates assets
locally. The second must fail if required inputs are absent or fail their
hash checks. Use `ASSETS=` and `NVIDIA_SOURCE=` to point at existing inputs.
The full tests still model hardware and firmware execution. The C harness
models the SHA-256 primitive; Python independently hashes the real assets.

The recorded sanitizer run disabled LeakSanitizer because process tracing was
restricted in the test environment. ASan/UBSan covered the C harness, not the
live kernel module or GPU firmware. CUDA numerical tests require actual GPU
access and are separate from this suite.

## Build a matched set of workload probes

Use a persistent build directory so the same binaries survive the restart:

```sh
mkdir -p build/matched-bench
nvcc -O3 -arch=sm_89 -std=c++17 -o build/matched-bench/mma_bench \
  diagnostics/bench/mma_bench.cu
nvcc -O3 -arch=sm_89 -std=c++17 -o build/matched-bench/validate_gemm \
  diagnostics/bench/validate_gemm.cu -lcublas
nvcc -O3 -arch=sm_89 -std=c++17 -o build/matched-bench/gemm_bench \
  docs/evidence/rtx4090-pr1-stock/gemm_bench.cu -lcublas
sha256sum build/matched-bench/mma_bench build/matched-bench/validate_gemm \
  build/matched-bench/gemm_bench > build/matched-bench/SHA256SUMS
```

The standalone large GEMM source is exactly the one used in the matched
report. Its simple inputs allow every output to be checked cheaply; use the
separate seeded validator for varied numerical inputs. Neither establishes
general application correctness. The [tensor-mode probe](../diagnostics/bench/README.md)
adds FP16/TF32 and unsigned-INT4 controls with exact device/cubin/power checks.

## Collect stock first, then unlocked

Start in a normal stock boot with the NVIDIA driver loaded. Record `uname -r`,
`/proc/cmdline`, GPU UUID, driver, existing power cap, and source/binary hashes.
Use the same exact GPU, kernel, driver, binaries, launch settings, clock policy
and cap for the unlocked run. Keep comparable background activity; record
whether a desktop is active. Do not change clock or power policies mid-pair.

For a single-GPU host, set `CUDA_VISIBLE_DEVICES` to that GPU's exact UUID
before the CUDA probes. The RM getter separately requires `--expect-uuid`.
Example collection after filling in your own UUID:

```sh
export CUDA_VISIBLE_DEVICES=GPU-PASTE-YOUR-EXACT-UUID
mkdir -p build/matched-bench/stock
uname -r > build/matched-bench/stock/kernel.txt
cat /proc/cmdline > build/matched-bench/stock/cmdline.txt
nvidia-smi --query-gpu=uuid,name,pci.bus_id,driver_version,power.limit \
  --format=csv > build/matched-bench/stock/gpu.csv
python3 tools/read_sm_rates.py --expect-uuid "$CUDA_VISIBLE_DEVICES" \
  > build/matched-bench/stock/sm-rates.json
sha256sum -c build/matched-bench/SHA256SUMS
build/matched-bench/validate_gemm > build/matched-bench/stock/numerical.txt
build/matched-bench/mma_bench 20000 5 > build/matched-bench/stock/mma.txt
build/matched-bench/gemm_bench > build/matched-bench/stock/gemm.jsonl
```

Stop if a command fails or any output reports mismatches. Capture clocks,
temperature and power while benchmarking, for example with `nvidia-smi`
sampling at 100 ms. Samples describe this short run; they do not establish
maximum power, long-term thermals or electrical safety.

Follow the [Quick Start](../QUICKSTART.md) for the firmware test, or use a
separate test image/entry containing the module and pinned assets. Keep the
normal entry available. With GRUB, inspect the actual entry's `linux` and
`initrd` paths; selecting ordinary Arch instead of a separate test entry
starts the ordinary boot. Our local test entry had a text-console `COLD`
confirmation; that is not a prompt installed by the standard Quick Start.

After the test boot, first confirm all of these:

- The running kernel and driver match the stock run.
- `/proc/cmdline` names the exact intended target.
- The current kernel journal contains that card's `gpu_unlock: unlocked` line.
- The RM getter reports the expected full rate (`fmla32=0` in this 4090 run;
  stock was `1`).
- NVIDIA initializes successfully and no `NVRM: Xid` errors are logged.

Repeat the same commands into an `unlocked` output directory, resetting the
GPU UUID environment variable after login. Check the binary hashes again.
Check the current kernel journal for Xid after the workloads as well.
Do not rerun the firmware transaction manually against a driver-owned card.

Report each shape/type and all timing repetitions. Distinguish instruction
throughput from GEMM medians and from application throughput. Include unchanged
or slower cases, output mismatch counts, telemetry and repeat counts. Before
publishing logs, remove UUIDs, PCI locations, hostnames, boot/root identifiers
and private paths while preserving numerical results.
