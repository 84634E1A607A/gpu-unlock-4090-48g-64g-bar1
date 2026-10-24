# Experimental RTX 4080 support

This change adds an opt-in path for NVIDIA GeForce RTX 4080 PCI ID `10de:2704` using the repository's existing AD102-family firmware assets. It is experimental and is disabled by default.

Enable it only with `gpu_unlock.rtx4080_experimental=1` and one exact `gpu_unlock.targets=<PCI address>` parameter. Broad target selection does not enable this profile. The existing AD103 experimental flag for PCI ID `10de:2705` does not select RTX 4080.

The path retains the firmware integrity checks, expected marker and permission checks, unlock status reporting, and post-reset validation. Unit tests exercise 32 GiB metadata, target routing, and failure handling with mocks; they do not establish hardware compatibility.

See [the hardware report](docs/rtx4080-hardware-report.md) for the scope and limits of the single-card trial.
