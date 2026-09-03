import importlib.util
import unittest
from pathlib import Path


RUNNER = Path(__file__).parents[1] / "paper-reproduction/run_a3dbsm_paper_campaign.py"
spec = importlib.util.spec_from_file_location("campaign_runner", RUNNER)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RunLoggingTests(unittest.TestCase):
    def test_log_path_is_unique_for_seed_and_byzantine_fraction(self):
        command_a = runner.build_command(60, "gm3d", 3001, 120.0, 1.0, "results/byz", 0.1)
        command_b = runner.build_command(60, "gm3d", 3001, 120.0, 1.0, "results/byz", 0.3)
        path_a = runner.run_log_path(Path("/artifact/ns-3"), command_a)
        path_b = runner.run_log_path(Path("/artifact/ns-3"), command_b)
        self.assertNotEqual(path_a, path_b)
        self.assertTrue(str(path_a).endswith("astro_n60_gm3d_s3001_bz10.log"))
        self.assertTrue(str(path_b).endswith("astro_n60_gm3d_s3001_bz30.log"))

    def test_manifest_records_log_path(self):
        command = runner.build_command(20, "gm3d", 3001, 10.0, 1.0, "results/main", 0.0)
        manifest = runner.build_manifests([command], Path("/artifact/ns-3"))["results/main"]
        self.assertEqual(manifest["runs"][0]["logFile"],
                         "results/main/astro_n20_gm3d_s3001_bz0.log")


if __name__ == "__main__":
    unittest.main()
