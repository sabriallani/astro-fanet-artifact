import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "paper-reproduction/summarize_a3dbsm_results.py"
spec = importlib.util.spec_from_file_location("summarizer", SCRIPT)
summarizer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summarizer)


HEADER = [
    "protocol", "nUavs", "mobility", "seed", "simTime", "pdr", "avgDelay",
    "throughput", "avgAoI", "ctrlOverhead", "energyPerBit", "brr",
    "broadcasts", "suppressed", "totalEnergy", "byzFraction",
]


class CsvValidationTests(unittest.TestCase):
    def write_csv(self, directory: Path, header: list[str], row: list[str]) -> None:
        with (directory / "run.csv").open("w", newline="") as handle:
            writer = csv.writer(handle)
            writer.writerow(header)
            writer.writerow(row)

    def test_load_rows_rejects_missing_required_columns(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            self.write_csv(directory, HEADER[:-1], ["astro"] * (len(HEADER) - 1))
            with self.assertRaisesRegex(ValueError, "missing required columns"):
                summarizer.load_rows(directory)

    def test_load_rows_rejects_non_numeric_metric(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            row = ["astro", "20", "gm3d", "3001", "10", "bad"] + ["0"] * 10
            self.write_csv(directory, HEADER, row)
            with self.assertRaisesRegex(ValueError, "non-numeric value"):
                summarizer.load_rows(directory)


if __name__ == "__main__":
    unittest.main()
