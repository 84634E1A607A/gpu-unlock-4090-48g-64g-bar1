# Latence.app: MSI RTX 4070 Ti AD104 → Windows

On 2026-10-24, the manual v0.5.4 transaction completed on one MSI RTX 4070 Ti (10de:2782 / 1462:5132), MSI Z490/MS-7C75 BIOS A.E0, i9-10850K, Windows build 26200.9168, NVIDIA 616.56. The operator confirmed Windows returned automatically after U, without intervening reset/power-off. Three independent Windows benchmark processes then passed.

| cuBLAS 4096² GEMM | Stock TFLOP/s | After TFLOP/s | Change |
|---|---:|---:|---:|
| FP32 pedantic | 30.23 | 30.53 | +1.0% |
| FP16/FP32 | 84.08 | 151.43 | +80.1% |
| BF16/FP32 | 84.03 | 148.25 | +76.4% |

Medians across three processes/phase, 15 samples × 5 products per mode and one second warmup. The strict comparator requires matching software/library/script hashes, GPU, driver, VBIOS, configured power, problem size and compute formats. Full 256² CPU FP64 checks and 1,024 sampled large-product elements passed; complete output hashes match stock. Large outputs were not exhaustively recomputed on CPU.

Stock was measured earlier that day. Temperature and background memory use differ. SM telemetry samples are about 2,940 MHz stock and 2,565–2,595 MHz after; memory 10,251 MHz and configured power 285 W in both phases. These are point samples, not continuous traces. Full cold A/B/A, longer stability and other cards remain unqualified. GEMM results are not register-resident issue rates or application-speedup promises.

## Implementation

The separate [Windows reference port](../windows/ad104-msi/README.md) adapts Ada transaction commit 491925c1079435e260137b9f3526066e764de830 and UEFI strategy from impacifist/Windows-GPU-UNLOCK commit 9352abd27480203ea146f766e7c96957912d148a. It does not change Linux target admission.

The exact MSI ROM/FWSEC, firmware inputs, geometry, loaded graphics code and driver ownership are pinned. UEFI callbacks release physical TextOut, GraphicsConsole and NVIDIA GOP consumers; an exclusive PCI claim and IO/MEM-only decode restoration follow. No driver code/global patch is used. The core requires firmware completion, HALT, payload marker, permissions, one SS0/SS1 update, full-rate readout and one FLR. Unsafe DMA remains retained until power-off.

Earlier attempts exposed normal periodic USB changes. v0.5.4 classifies the exact onboard USB branch before detach; USB device/child edges may disappear/reappear, while host driver/PCI ownership stays mandatory. Input child additions require the original splitter, matching BY_DRIVER owner and bounded USB host path. GPU/display ownership, virtual console callbacks, non-USB graph invariants and DMA containment remain enforced. Host tests reproduce late additions and a 165 → 152 relationship transition.

The successful log records `result=0 safe=1 retained=0 started=1 quiesced=1 flrs=1 DMA_live=0`. SS0/SS1 persist as 0x88888888/0x8 after FLR; effective readout is zero. Windows is staged from a pinned read-only ESP image. Final checks after USB close and StartImage are unlogged; operator-confirmed Windows recovery and the subsequent benchmarks supplement the transaction journal.

## Publication

Historical hardware EFI SHA-256: `b27709643f57284bf5d1e8941caa8f9b12725d7b32c5a9863e89ebafd456a37f`.

No binary EFI is distributed. The public source moves the local ESP GUID to an excluded header, uses a synthetic GUID only for host I/O tests and makes tool paths configurable. With the original private configuration, it rebuilds the **same hardware EFI byte-for-byte**. All **177** scenarios were freshly rerun on the publication tree with private inputs.

[Sanitized evidence](evidence/ad104-windows/README.md) includes six unchanged benchmark JSONs, a comparison, validation metadata and a journal with volatile handle addresses replaced. ROM, firmware, loaded code, Windows executables, real partition GUIDs and personal paths are excluded. Diagnostic capture/source and reproduction steps are included with the port.

**Latence.app** maintains this contribution. Original upstream credits and licences remain. A public fork/PR is a proposed contribution, not upstream maintainer approval.
