# AD104 / Windows evidence — Latence.app

Six benchmark JSONs are unchanged: three `stock-*` and three `unlocked-v054-*` measurements. `comparison.md` comes from the strict comparator; `validation.json` records scope, conditions and limits. `RUN0007.sanitized.txt` preserves ordering, register values and transaction results while replacing volatile handle addresses by consistent aliases and omitting the endpoints line. There are no firmware/ROM/Windows binaries or personal paths here.

`SHA256SUMS.json` covers the evidence data files, excluding this README. The benchmark SHA-256 recorded in all six measurements matches `windows/ad104-msi/benchmark.py`.

Recheck from the port directory:

```powershell
$e = '../../docs/evidence/ad104-windows'
$a = @(Get-ChildItem "$e/stock-*.json" | Sort-Object Name | ForEach-Object FullName)
$b = @(Get-ChildItem "$e/unlocked-v054-*.json" | Sort-Object Name | ForEach-Object FullName)
python compare.py --before @a --after @b
```

This is initial evidence on one machine. Cold A/B/A, long-duration stability and general compatibility remain unqualified. See the [report](../../ad104-windows-latence-app.md).
