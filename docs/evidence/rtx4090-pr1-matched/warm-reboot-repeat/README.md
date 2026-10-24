# RTX 4090 warm-reboot repeat

This is a second unlocked run after an ordinary warm reboot. The selected card reported `gpu_unlock: unlocked` before the NVIDIA driver initialized. The public RM selector readback remained `fmla32=0` (full rate), and the benchmark suite and journal check passed. The selected boot-log line is in [warm-boot-excerpt.txt](warm-boot-excerpt.txt).

This run uses the original stock measurement in the parent evidence directory as its comparison baseline. It is one repeat unlocked measurement, not another matched stock/unlocked pair.

## Results

The best-of-five instruction probe reported 320.4 TF/s FP16/FP32 and 318.8 TF/s BF16/FP32, approximately 2× the existing stock values. Scalar FP32, FP16/FP16 and integer controls remained near their prior levels. The GEMM medians compared with the existing stock run were:

| N | Input | Stock TF/s | Warm-reboot unlocked TF/s | Ratio |
|---:|---|---:|---:|---:|
| 1024 | FP16 | 107.546 | 124.830 | 1.161× |
| 1024 | BF16 | 127.100 | 150.031 | 1.180× |
| 4096 | FP16 | 153.234 | 198.709 | 1.297× |
| 4096 | BF16 | 153.418 | 140.822 | 0.918× |
| 8192 | FP16 | 168.675 | 189.973 | 1.126× |
| 8192 | BF16 | 166.197 | 191.399 | 1.152× |

All six seeded numerical cases and all large GEMM output checks passed with zero mismatches. The 4096 BF16 result regressed in this run; the measurements show that near-2× instruction issue rate does not guarantee a faster application GEMM.

The telemetry CSV contains a short sample window. The maximum sampled active-load power was 300.85 W, temperature 75 °C, and SM clock 2670 MHz. This is not a maximum-power or long-duration thermal test.

See [metadata.json](metadata.json) for run conditions, ratios, checks and limitations. `mma.txt`, `gemm.jsonl`, `numerical.txt`, `sm-rates.json`, `tensor-modes.json`, and `telemetry.csv` preserve the sanitized run output. Verify included files with `sha256sum -c SHA256SUMS`.
