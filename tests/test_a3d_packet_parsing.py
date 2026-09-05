import unittest
from pathlib import Path


class A3dPacketParsingTests(unittest.TestCase):
    def test_broadcast_route_strips_udp_before_reading_astro_header(self):
        source = (
            Path(__file__).resolve().parents[1]
            / "ns-allinone-3.29"
            / "ns-3.29"
            / "src"
            / "astro-fanet"
            / "model"
            / "astro-routing-protocol.cc"
        ).read_text()
        self.assertIn('#include "ns3/udp-header.h"', source)
        self.assertIn("UdpHeader udpHeader;", source)
        self.assertIn("pCopy->RemoveHeader (udpHeader)", source)


if __name__ == "__main__":
    unittest.main()
