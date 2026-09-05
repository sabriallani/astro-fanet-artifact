import unittest
from pathlib import Path


class WafIntegrityTests(unittest.TestCase):
    def test_bundled_waf_archive_is_not_unicode_corrupted(self):
        waf_path = (
            Path(__file__).resolve().parents[1]
            / "ns-allinone-3.29"
            / "ns-3.29"
            / "waf"
        )
        data = waf_path.read_bytes()
        self.assertIn(b"#==>\n", data)
        self.assertIn(b"#<==\n", data)
        self.assertNotIn(b"\xef\xbf\xbd", data)


if __name__ == "__main__":
    unittest.main()
