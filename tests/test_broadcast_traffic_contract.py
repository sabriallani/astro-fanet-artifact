import unittest
from pathlib import Path


SCENARIO = Path(__file__).parents[1] / "ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc"


class BroadcastTrafficContractTests(unittest.TestCase):
    def test_suppression_policies_broadcast_all_traffic_classes(self):
        source = SCENARIO.read_text()
        self.assertIn("broadcastAllTraffic", source)
        self.assertIn("dataHdr.SetIsBroadcast (m_broadcastAllTraffic)", source)
        self.assertIn("isSuppressionPolicy && broadcastAllTraffic", source)


if __name__ == "__main__":
    unittest.main()
