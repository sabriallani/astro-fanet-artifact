import unittest
from pathlib import Path


SCENARIO = Path(__file__).parents[1] / "ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc"


class SecurityMetricContractTests(unittest.TestCase):
    def test_csv_exposes_byzantine_drop_count(self):
        source = SCENARIO.read_text()
        self.assertIn("byzantineDrops", source)
        self.assertIn("g_metrics.byzantineDrops", source)


if __name__ == "__main__":
    unittest.main()
