import importlib.util
import unittest
from pathlib import Path


AGGREGATOR = Path(__file__).parents[1] / "paper-reproduction/aggregate_baseline_campaign.py"
spec = importlib.util.spec_from_file_location("baseline_aggregator", AGGREGATOR)
aggregator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aggregator)


class BaselineAggregationTests(unittest.TestCase):
    def test_byzantine_fractions_are_separate_summary_cells(self):
        rows = [
            {
                "protocol": "astro",
                "nUavs": "60",
                "mobility": "gm3d",
                "byzFraction": "0.0",
                "seed": "3001",
                "pdr": "50.0",
            },
            {
                "protocol": "astro",
                "nUavs": "60",
                "mobility": "gm3d",
                "byzFraction": "0.2",
                "seed": "3001",
                "pdr": "40.0",
            },
        ]
        summary = aggregator.summarise(rows)
        self.assertEqual(len(summary), 2)
        self.assertEqual(
            {(row["byzFraction"], row["pdr_mean"]) for row in summary},
            {("0.0", "50.000000"), ("0.2", "40.000000")},
        )


if __name__ == "__main__":
    unittest.main()
