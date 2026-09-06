"""Source-level contract tests for the executable broadcast-suppression baselines.

The manuscript compares A3D-BSM against four broadcast-suppression families.
Before this module existed the scenario only exposed `astro`, `aodv`, `olsr`,
`epidemic` and `dqn`, where `epidemic` and `dqn` were AODV with tweaked flags.
That is not a suppression baseline: it cannot answer "does A3D-BSM suppress
better than simple flooding / counter-based / probabilistic / distance-based
schemes?".

These tests pin the contract of the real baseline layer:

* SF   -- Simple Flooding: every node rebroadcasts every new packet once.
* PR   -- Probabilistic Rebroadcast: rebroadcast with fixed probability p.
* CB   -- Counter-Based: suppress if >= C duplicates heard during a jitter window.
* SBA  -- Scalable Broadcast Algorithm: suppress if the candidate adds no
          new neighbour coverage relative to the previous relay.

They are source-level invariants because ns-3 cannot be compiled in this
environment; a real build + run happens in CI.
"""

import pathlib
import unittest


ROOT = pathlib.Path(__file__).parents[1]
NS3 = ROOT / "ns-allinone-3.29/ns-3.29"
BASELINE_H = NS3 / "src/astro-fanet/model/broadcast-baselines.h"
BASELINE_CC = NS3 / "src/astro-fanet/model/broadcast-baselines.cc"
ROUTING_H = NS3 / "src/astro-fanet/model/astro-routing-protocol.h"
ROUTING_CC = NS3 / "src/astro-fanet/model/astro-routing-protocol.cc"
SCENARIO = NS3 / "scratch/astro-fanet-sim.cc"
WSCRIPT = NS3 / "src/astro-fanet/wscript"


class BaselineModuleTests(unittest.TestCase):
    def test_baseline_sources_exist(self):
        self.assertTrue(BASELINE_H.exists(), "broadcast-baselines.h missing")
        self.assertTrue(BASELINE_CC.exists(), "broadcast-baselines.cc missing")

    def test_baseline_sources_are_built(self):
        text = WSCRIPT.read_text()
        self.assertIn("model/broadcast-baselines.cc", text)
        self.assertIn("model/broadcast-baselines.h", text)

    def test_all_four_suppression_families_are_declared(self):
        text = BASELINE_H.read_text()
        for token in (
            "BASELINE_NONE",
            "BASELINE_SIMPLE_FLOODING",
            "BASELINE_PROBABILISTIC",
            "BASELINE_COUNTER_BASED",
            "BASELINE_SBA",
        ):
            self.assertIn(token, text, f"{token} not declared")

    def test_simple_flooding_always_rebroadcasts_once(self):
        """SF must not consult density, probability or coverage."""
        text = BASELINE_CC.read_text()
        text = text[text.index("BroadcastBaselineEngine::DecideImmediate"):]
        start = text.index("case BASELINE_SIMPLE_FLOODING:")
        # Scope strictly to the SF arm, otherwise the window bleeds into PR.
        window = text[start:text.index("case BASELINE_PROBABILISTIC:", start)]
        self.assertIn("return true", window)
        for forbidden in ("m_rebroadcastProbability", "AdditionalCoverage", "density"):
            self.assertNotIn(forbidden, window)

    def test_counter_based_uses_duplicate_threshold(self):
        text = BASELINE_CC.read_text()
        self.assertIn("m_counterThreshold", text)
        self.assertIn("duplicateCount", text)

    def test_probabilistic_uses_uniform_draw_against_probability(self):
        text = BASELINE_CC.read_text()
        self.assertIn("m_rebroadcastProbability", text)
        self.assertIn("UniformRandomVariable", text)

    def test_sba_uses_additional_neighbour_coverage(self):
        """SBA rebroadcasts only when it reaches neighbours the previous relay missed.

        AdditionalCoverage is a template helper, so it lives in the header.
        """
        header = BASELINE_H.read_text()
        self.assertIn("AdditionalCoverage", header)
        self.assertIn("m_commRange", header)
        decide = BASELINE_CC.read_text()
        decide = decide[decide.index("BroadcastBaselineEngine::DecideAfterRad"):]
        start = decide.index("case BASELINE_SBA:")
        self.assertIn("additionalCover > 0", decide[start:start + 300])

    def test_baselines_are_deterministic_per_seed(self):
        """The probabilistic baseline must derive its stream from the run seed."""
        text = BASELINE_CC.read_text()
        self.assertIn("SetStream", text)


class BaselineWiringTests(unittest.TestCase):
    def test_routing_protocol_exposes_baseline_mode(self):
        text = ROUTING_H.read_text()
        self.assertIn("SetBaselineMode", text)
        self.assertIn("BroadcastBaseline", text)

    def test_baseline_decision_replaces_a3d_decision_in_same_path(self):
        """A baseline run must reuse the identical forwarding path as A3D-BSM.

        Otherwise the comparison measures scenario differences, not algorithms.
        """
        text = ROUTING_CC.read_text()
        decision = text.index("AstroAction bcastDecision")
        baseline = text.index("m_baseline", decision - 3000)
        rebroadcast = text.index("BroadcastPacket (p->Copy (), header)", decision)
        self.assertLess(baseline, rebroadcast)

    def test_progress_guard_is_skipped_for_baselines(self):
        """The progress-aware guard is an A3D-BSM contribution, not a baseline feature."""
        text = ROUTING_CC.read_text()
        # A baseline run returns from the suppression block before the
        # progress-aware guard, so the guard stays an A3D-BSM-only feature.
        baseline_block = text.index("if (m_baseline.IsActive ())")
        guard = text.index("IsProgressingRelay (myPos, prevRelay)")
        self.assertLess(baseline_block, guard)
        between = text[baseline_block:guard]
        self.assertIn("return true;", between)

    def test_scenario_exposes_baseline_protocol_selectors(self):
        text = SCENARIO.read_text()
        for selector in ('"sf"', '"pr"', '"cb"', '"sba"'):
            self.assertIn(selector, text, f"selector {selector} not exposed")

    def test_scenario_rejects_unknown_protocol(self):
        text = SCENARIO.read_text()
        self.assertIn("Unknown protocol", text)


if __name__ == "__main__":
    unittest.main()
