import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("boot_option", Path(__file__).resolve().parents[1] / "tools/print_boot_option.py")
boot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(boot)

class AD104BootOptionTests(unittest.TestCase):
    def test_exact_id_only(self):
        output = "0000:01:00.0 VGA [10de:2786]\n0000:02:00.0 VGA [10de:2782]\n0000:11:00.0 VGA [1002:164e]"
        self.assertEqual(boot.candidates(output), [("0000:01:00.0", "2786")])
