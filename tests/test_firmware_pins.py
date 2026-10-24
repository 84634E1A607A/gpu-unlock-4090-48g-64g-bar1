# SPDX-License-Identifier: GPL-2.0-only
"""Keep the kernel's runtime asset identities in sync with build manifests."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FirmwarePinTests(unittest.TestCase):
    def test_runtime_pins_match_manifest(self):
        manifest = json.loads((ROOT / "firmware/artifacts-610.43.03.json").read_text())
        expected = dict(manifest["derived_assets"])
        expected["gsp_ga10x.bin"] = manifest["source_artifacts"][2]["firmware"][0]["sha256"]
        for key in ("generated_payload", "generated_ad102_payload"):
            entry = manifest[key]
            expected[entry["filename"]] = entry["sha256"]
        source = (ROOT / "kernel/gpu_unlock/firmware_pins_610.h").read_text()
        entries = re.findall(r'\{ "gpu-unlock/610\.43\.03/([^"\n]+)", \{([^}]+)\}', source)
        self.assertEqual(len(entries), len(expected))
        actual = {name: bytes(int(word, 16) for word in re.findall(r"0x[0-9a-f]{2}", data)).hex()
                  for name, data in entries}
        self.assertEqual(actual, expected)
