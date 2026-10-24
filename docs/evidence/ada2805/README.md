# RTX 4060 Ti 16GB 10de:2805 evidence

`tensor-modes-unlocked.json` is unedited stdout from
`diagnostics/bench/tensor_modes_driver_runner.py` on one card at its 165 W
default cap, except that the GPU UUID is replaced with `GPU-UUID-OMITTED`.
No same-card stock run has been made.

`sm-rates-stock.json` and `sm-rates-unlocked.json` are output from the
read-only `tools/read_sm_rates.py`, with the UUID replaced the same way. The
stock reading was taken in an earlier boot, before this build was installed;
the stock 1.1.1 module present then does not accept `10de:2805`.

`transaction-excerpt.txt` contains this card's current-boot kernel messages
from the `entry:` line to `gpu_unlock: unlocked`, with ROM/FWSEC parsing,
asset loading and Falcon DMA lines removed. It is an excerpt, not a complete
kernel log, and contains no host name.

`SHA256SUMS` covers every other file in this directory.
