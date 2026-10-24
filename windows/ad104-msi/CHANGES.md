# Latence.app AD104 Windows changes — 2026-10-24

- Adapt the pinned Ada transaction to checked UEFI PCI I/O, DMA and explicit error/retention state for the captured AD104 profile.
- Add MSI console/GOP ownership capture/validation and callback-based release, exclusive PCI claim and IO/MEM-only restoration.
- Preload a pinned Windows manager from a read-only ESP, require U for one attempt and contain all post-mutation failures.
- Handle periodic USB additions/removals within bounded preflight-classified USB ownership, while preserving GPU/display/console and host ownership invariants.
- Require payload marker, HALT, permission and selector/readout checks before/after one FLR; free DMA only after verified quiescence.
- Record one successful Windows handoff and three numerical/throughput runs; retain exact scope and pending A/B/A/long-duration qualification.
- Publish source-only with an ignored local ESP header, synthetic mock-test GUID, configurable compiler paths, diagnostic source and sanitized evidence. The configured public source rebuilds the historical hardware EFI byte-for-byte; 177 scenarios freshly pass.

Upstream commits and full notices remain in provenance.json and licence files. No Linux target admission changes.
