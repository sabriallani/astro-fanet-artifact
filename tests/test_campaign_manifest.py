import importlib.util
import unittest
from pathlib import Path


RUNNER = Path(__file__).parents[1] / "paper-reproduction/run_a3dbsm_paper_campaign.py"


spec = importlib.util.spec_from_file_location("campaign_runner", RUNNER)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class CampaignManifestTests(unittest.TestCase):
    def test_manifest_groups_runs_by_output_directory(self):
        tasks = [
            runner.build_command(20, "gm3d", 3001, 10.0, 1.0, "results/main", 0.0),
            runner.build_command(20, "gm3d", 3002, 10.0, 1.0, "results/main", 0.0),
            runner.build_command(60, "gm3d", 3001, 10.0, 1.0, "results/byz", 0.3),
        ]
        manifests = runner.build_manifests(tasks, Path("/artifact/ns-3"))
        self.assertEqual(set(manifests), {"results/main", "results/byz"})
        self.assertEqual(len(manifests["results/main"]["runs"]), 2)
        self.assertEqual(len(manifests["results/byz"]["runs"]), 1)

    def test_manifest_preserves_exact_command_and_parameters(self):
        task = runner.build_command(60, "gm3d", 3001, 120.0, 1.0, "results/byz", 0.3)
        manifest = runner.build_manifests([task], Path("/artifact/ns-3"))["results/byz"]
        run = manifest["runs"][0]
        self.assertEqual(run["nUavs"], 60)
        self.assertEqual(run["mobility"], "gm3d")
        self.assertEqual(run["seed"], 3001)
        self.assertEqual(run["byzFraction"], 0.3)
        self.assertEqual(run["command"], task)


if __name__ == "__main__":
    unittest.main()
