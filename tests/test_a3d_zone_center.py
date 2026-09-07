import unittest
from pathlib import Path


SOURCE = (Path(__file__).parents[1]
          / "ns-allinone-3.29/ns-3.29/src/astro-fanet/model/a3d-bsm.cc")


class A3dZoneCenterTests(unittest.TestCase):
    def test_rebroadcast_zone_is_centered_on_previous_relay(self):
        source = SOURCE.read_text()
        self.assertIn("Vector3D zoneCenter = prevRelayPos", source)
        self.assertIn("IsInsideZone (currentPos, zoneCenter, semiAxes)", source)


if __name__ == "__main__":
    unittest.main()
