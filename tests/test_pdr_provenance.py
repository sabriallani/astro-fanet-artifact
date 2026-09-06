import unittest
from pathlib import Path


class PdrProvenanceTests(unittest.TestCase):
    def test_scenario_exports_pdr_provenance_and_raw_counters(self):
        source = (
            Path(__file__).resolve().parents[1]
            / "ns-allinone-3.29"
            / "ns-3.29"
            / "scratch"
            / "astro-fanet-sim.cc"
        ).read_text()
        for field in (
            "pdr_source",
            "totalGenerated",
            "totalDelivered",
            "flowMonitorDelivered",
            "flowMonitorLost",
        ):
            self.assertIn(field, source)

    def test_pdr_is_recomputed_after_flowmonitor_fallback(self):
        source = (
            Path(__file__).resolve().parents[1]
            / "ns-allinone-3.29"
            / "ns-3.29"
            / "scratch"
            / "astro-fanet-sim.cc"
        ).read_text()
        fallback = source.index("if (fmDelivered > 0 && g_metrics.totalDelivered == 0)")
        csv_output = source.index("csv << \"protocol,nUavs")
        self.assertLess(fallback, csv_output)
        self.assertIn("pdrSource", source)


if __name__ == "__main__":
    unittest.main()
