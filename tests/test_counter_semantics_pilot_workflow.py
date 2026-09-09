"""Contract for the bounded GitHub validation of broadcast counter semantics."""

from pathlib import Path
import unittest


WORKFLOW = (Path(__file__).parents[1]
            / ".github/workflows/counter-semantics-pilot.yml")


class CounterSemanticsPilotWorkflowTests(unittest.TestCase):
    def test_pilot_is_manual_bounded_and_uses_unique_evidence_paths(self):
        source = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch:", source)
        self.assertIn("push:", source)
        self.assertIn("experiment/a3d-dpms-trajectory-pilot", source)
        self.assertIn(".github/workflows/counter-semantics-pilot.yml", source)
        self.assertIn("counter-semantics-pilot", source)
        self.assertIn("results/counter-semantics-pilot", source)
        self.assertIn("for protocol in astro sf cb", source)
        self.assertIn("for mobility in gm3d rpgm", source)
        self.assertIn("for seed in 3001 3002", source)
        self.assertIn("broadcasts != rebroadcasts", source)
        self.assertIn("run-manifest.json", source)
        manifest_heredoc = source[source.index("run-manifest.json"):]
        self.assertIn("\n          import json\n", manifest_heredoc)
        self.assertIn("GITHUB_SHA", source)
        self.assertIn("--broadcastAllTraffic=true", source)
        self.assertIn("actions/upload-artifact@v4", source)
        self.assertNotIn("git push origin", source)


if __name__ == "__main__":
    unittest.main()
