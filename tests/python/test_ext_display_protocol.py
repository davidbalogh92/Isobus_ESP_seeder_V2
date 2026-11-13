"""A külső kijelző protokoll fejléce a kijelző repó változatlan másolata kell legyen.

Frissítés kizárólag a tools/sync_ext_display_protocol.sh szkripttel (az a rögzített ellenőrzőösszeget is frissíti)."""
import hashlib
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class ExtDisplayProtocolCopy(unittest.TestCase):
    def test_header_unchanged(self):
        data = (ROOT / "protocol/ext_display_protocol.h").read_bytes()
        expected = (ROOT / "protocol/ext_display_protocol.sha256").read_text().strip()
        self.assertEqual(hashlib.sha256(data).hexdigest(), expected,
                         "a protocol/ext_display_protocol.h eltér a rögzített (kijelző repóbeli) példánytól")

    def test_spec_present(self):
        self.assertIn("Proprietary A", (ROOT / "docs/ext_display_spec.md").read_text("utf-8"))


if __name__ == "__main__":
    unittest.main()
