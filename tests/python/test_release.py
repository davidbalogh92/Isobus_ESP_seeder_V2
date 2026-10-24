import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from build_release import BOARDS, fw_version, merge_bin, validate_partition_table  # noqa: E402


class Release(unittest.TestCase):
    def test_partition_layout(self):
        for env in BOARDS:
            validate_partition_table(env)

    def test_version_parsed(self):
        self.assertRegex(fw_version(), r"^\d+\.\d+\.\d+")

    def test_merge_padding(self):
        with tempfile.TemporaryDirectory() as t:
            d = Path(t)
            a, b, out = d / "a.bin", d / "b.bin", d / "o.bin"
            a.write_bytes(b"\xe9BOOT")
            b.write_bytes(b"\xe9APP")
            merge_bin([(0, a), (0x20000, b)], out)
            data = out.read_bytes()
            self.assertEqual(data[:5], b"\xe9BOOT")
            self.assertEqual(data[0x20000:], b"\xe9APP")
            self.assertEqual(set(data[5:0x20000]), {0xFF})

    def test_overlap_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            d = Path(t)
            a, b = d / "a", d / "b"
            a.write_bytes(b"A" * 20)
            b.write_bytes(b"B" * 20)
            with self.assertRaises(ValueError):
                merge_bin([(0, a), (10, b)], d / "o")


if __name__ == "__main__":
    unittest.main()
