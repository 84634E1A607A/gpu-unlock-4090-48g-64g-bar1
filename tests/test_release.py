# SPDX-License-Identifier: GPL-2.0-only
"""Host checks for the retained module components."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
KERNEL = ROOT / "kernel/gpu_unlock"
# Generated or installed firmware lets the adapter run the whole unlock
# against mocked hardware.
ASSETS = next((path for path in (Path(os.environ.get("GPU_UNLOCK_ASSETS", "")),
                                 Path("/usr/lib/firmware/gpu-unlock/610.43.03"))
               if (path / "gsp_ga10x.bin").is_file()), None)

CASES = {
    "gfw_eval": ["gfw_eval.c"],
    "layout_610": ["layout_610.c"],
    "prep_610": ["layout_610.c", "prep_610.c"],
    "vbios_610": ["vbios_610.c"],
    "unlock_adapter": ["gfw_eval.c", "prep_610.c", "layout_610.c", "vbios_610.c"],
    "ad102_kernel": ["gfw_eval.c", "prep_610.c", "layout_610.c", "vbios_610.c"],
    "main": [],
    "bench_helpers": [],
}
# Tests that include a kernel source directly build it against a shim.
SHIMS = {
    "unlock_adapter": ["-I", str(ROOT / "tests/unlock_shim")],
    "ad102_kernel": ["-I", str(ROOT / "tests/unlock_shim")],
    # The kernel build does not warn on sign-compare; match it for main.c.
    "main": ["-I", str(ROOT / "tests/main_shim"), "-pthread", "-Wno-sign-compare"],
}


class ComponentTests(unittest.TestCase):
    def run_component(self, name, args=()):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / name
            command = [os.environ.get("CC", "cc"), "-std=c11", "-Wall",
                       "-Wextra", "-Werror", "-Wno-unused-parameter",
                       "-I", str(KERNEL), *SHIMS.get(name, []),
                       *(str(KERNEL / source) for source in CASES[name]),
                       str(ROOT / "tests" / f"test_{name}.c"),
                       "-o", str(output)]
            subprocess.run(command, check=True)
            result = subprocess.run([str(output), *args], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0,
                             (result.stdout + result.stderr)[-4000:])

    def test_components(self):
        for name in CASES:
            with self.subTest(name=name):
                self.run_component(name)


class PinnedTransactionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        required = os.environ.get("GPU_UNLOCK_REQUIRE_INPUTS") == "1"
        if ASSETS is None:
            message = "full transactions require GPU_UNLOCK_ASSETS with generated firmware"
            if required:
                raise AssertionError(message)
            raise unittest.SkipTest(message)
        if required:
            source = Path(os.environ.get("GPU_UNLOCK_NVIDIA_SOURCE", "/nonexistent"))
            if not (source / "src/nvidia/generated").is_dir():
                raise AssertionError("full validation requires GPU_UNLOCK_NVIDIA_SOURCE")
        # The C harness models SHA-256 responses, not the kernel crypto library.
        # Independently verify the bytes before running it with real assets.
        manifest = json.loads((ROOT / "firmware/artifacts-610.43.03.json").read_text())
        pins = dict(manifest["derived_assets"])
        pins["gsp_ga10x.bin"] = manifest["source_artifacts"][2]["firmware"][0]["sha256"]
        for key in ("generated_payload", "generated_ad102_payload"):
            entry = manifest[key]
            pins[entry["filename"]] = entry["sha256"]
        for name, expected in pins.items():
            path = ASSETS / name
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != expected:
                raise AssertionError(f"asset SHA-256 mismatch: {path}")

    def test_ga102_full_transactions(self):
        ComponentTests.run_component(self, "unlock_adapter", ("-", str(ASSETS)))

    def test_ad102_full_transactions(self):
        ComponentTests.run_component(self, "ad102_kernel", ("-", str(ASSETS)))

    def test_shared_gsp_sections(self):
        ComponentTests.run_component(self, "prep_610", (str(ASSETS),))


class CoveragePreflightTests(unittest.TestCase):
    def test_missing_assets_are_an_explicit_skip(self):
        with mock.patch(f"{__name__}.ASSETS", None), \
                mock.patch.dict(os.environ, {"GPU_UNLOCK_REQUIRE_INPUTS": "0"}):
            with self.assertRaisesRegex(unittest.SkipTest, "full transactions require"):
                PinnedTransactionTests.setUpClass()

    def test_required_missing_assets_fail(self):
        with mock.patch(f"{__name__}.ASSETS", None), \
                mock.patch.dict(os.environ, {"GPU_UNLOCK_REQUIRE_INPUTS": "1"}):
            with self.assertRaisesRegex(AssertionError, "full transactions require"):
                PinnedTransactionTests.setUpClass()

    def test_required_missing_source_fails(self):
        with mock.patch(f"{__name__}.ASSETS", ROOT), \
                mock.patch.dict(os.environ, {"GPU_UNLOCK_REQUIRE_INPUTS": "1",
                                             "GPU_UNLOCK_NVIDIA_SOURCE": "/nonexistent"}):
            with self.assertRaisesRegex(AssertionError, "GPU_UNLOCK_NVIDIA_SOURCE"):
                PinnedTransactionTests.setUpClass()
