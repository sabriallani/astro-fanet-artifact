"""Contract tests for suppression-layer metric isolation.

The first baseline campaign (run 34039208151) produced identical broadcast and
suppression counters for A3D-BSM, SF and SBA.  Root cause: the rebroadcast path
reported `m_suppressedBroadcasts`, a counter the policy engine also increments
in ExecuteAction().  Aggregating both sources made every scheme look the same
and reported SP ~= 41% for simple flooding, which never suppresses by
definition.

These tests pin the invariants at source level.  The simulator cannot be
compiled in this environment (no g++/make), so they are contract tests over the
C++ sources; the CI build is what proves the code compiles.
"""

import pathlib
import re
import unittest

REPO = pathlib.Path(__file__).resolve().parents[1]
MODEL = REPO / "ns-allinone-3.29" / "ns-3.29" / "src" / "astro-fanet" / "model"
PROTO_CC = MODEL / "astro-routing-protocol.cc"
PROTO_H = MODEL / "astro-routing-protocol.h"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def broadcast_block(source):
    """Return the data-broadcast rebroadcast-decision block."""
    start = source.find("Now check if this is a data broadcast")
    assert start != -1, "data broadcast block not found"
    end = source.find("Unicast forwarding", start)
    assert end != -1, "end of broadcast block not found"
    return source[start:end]


class TestSuppressionCounterIsolation(unittest.TestCase):
    """The rebroadcast path must not share a counter with the policy engine."""

    def test_dedicated_rebroadcast_counters_declared(self):
        header = read(PROTO_H)
        for member in ("m_rebroadcasts", "m_rebroadcastSuppressions"):
            self.assertIn(
                member, header,
                f"{member} must exist so rebroadcast decisions are counted "
                "separately from ExecuteAction()'s m_suppressedBroadcasts",
            )

    def test_execute_action_does_not_touch_rebroadcast_counters(self):
        source = read(PROTO_CC)
        start = source.find("AstroRoutingProtocol::ExecuteAction")
        self.assertNotEqual(start, -1, "ExecuteAction not found")
        end = source.find("\n}", start)
        body = source[start:end]
        for member in ("m_rebroadcasts", "m_rebroadcastSuppressions"):
            self.assertNotIn(
                member, body,
                f"ExecuteAction must not modify {member}; that conflation is "
                "the bug that made SF and A3D-BSM report identical counters",
            )

    def test_rebroadcast_path_uses_dedicated_counters(self):
        block = broadcast_block(read(PROTO_CC))
        self.assertIn("m_rebroadcastSuppressions++", block)
        self.assertIn("m_rebroadcasts++", block)

    def test_saved_ratio_uses_rebroadcast_decisions_only(self):
        header = read(PROTO_H)
        start = header.find("GetSavedRebroadcastRatio")
        self.assertNotEqual(start, -1, "GetSavedRebroadcastRatio missing")
        body = header[start:start + 400]
        self.assertIn("m_rebroadcastSuppressions", body)
        self.assertNotIn(
            "m_suppressedBroadcasts", body,
            "SP must be derived from rebroadcast decisions only",
        )


class TestSimpleFloodingNeverSuppresses(unittest.TestCase):
    """SF is the no-suppression reference; a nonzero SP means a wiring bug."""

    def test_baseline_hook_precedes_suppression_accounting(self):
        block = broadcast_block(read(PROTO_CC))
        hook = block.find("m_baseline")
        suppress = block.find("m_rebroadcastSuppressions++")
        self.assertNotEqual(hook, -1, "baseline policy is not consulted")
        self.assertNotEqual(suppress, -1, "no rebroadcast suppression counted")
        self.assertLess(
            hook, suppress,
            "the baseline decision must be taken before suppression is "
            "recorded, otherwise SF is credited with suppressions",
        )

    def test_duplicate_return_is_not_counted_as_suppression(self):
        """A duplicate is dropped by the cache, not by a suppression policy."""
        block = broadcast_block(read(PROTO_CC))
        idx = block.find("m_duplicateReceptions++")
        self.assertNotEqual(idx, -1, "duplicate receptions are not counted")
        window = block[idx:idx + 320]
        self.assertNotIn(
            "m_rebroadcastSuppressions++", window,
            "cache duplicates must not inflate SP; this is why SF reported "
            "SP ~= 41% while never suppressing",
        )


class TestScalableBroadcastIsEffective(unittest.TestCase):
    """SBA defers then re-evaluates; it must not collapse onto flooding."""

    def test_deferred_decision_entry_point_exists(self):
        self.assertIn(
            "BaselineDeferredDecision", read(PROTO_H),
            "SBA/CB need a deferred callback, otherwise they behave like SF",
        )

    def test_rad_policies_are_routed_to_the_deferred_path(self):
        block = broadcast_block(read(PROTO_CC))
        self.assertIn(
            "BaselineUsesRad", block,
            "CB/SBA must be detected as deferred policies",
        )

    def test_deferred_decision_is_scheduled(self):
        block = broadcast_block(read(PROTO_CC))
        self.assertIn("Simulator::Schedule", block)
        self.assertIn("BaselineDeferredDecision", block)

    def test_deferred_path_bypasses_the_duplicate_cache(self):
        """The re-broadcast after the delay must not be eaten by the cache."""
        source = read(PROTO_CC)
        # Anchor on the definition, not the Simulator::Schedule call site.
        start = source.find("void\nAstroRoutingProtocol::BaselineDeferredDecision")
        self.assertNotEqual(start, -1, "BaselineDeferredDecision not defined")
        body = source[start:start + 2400]
        self.assertIn(
            "DecideAfterRad", body,
            "the deferred callback must consult the baseline's coverage rule",
        )
        self.assertIn(
            "AdditionalCoverage", body,
            "SBA must compute additional neighbour coverage, else it is SF",
        )
        self.assertIn(
            "BroadcastPacket",
            body,
            "the deferred rebroadcast must actually be re-injected",
        )


class TestEmergencyAccounting(unittest.TestCase):
    """ENSR is the empirical test of the priority theorem."""

    def test_emergency_counters_split_forwarded_and_suppressed(self):
        block = broadcast_block(read(PROTO_CC))
        self.assertIn("m_emergencyForwarded++", block)
        self.assertIn("m_emergencySuppressed++", block)

    def test_ensr_is_not_hardcoded(self):
        header = read(PROTO_H)
        start = header.find("GetEmergencyNonSuppressionRate")
        self.assertNotEqual(start, -1)
        body = header[start:start + 400]
        self.assertNotIn(
            "return 100.0;", body,
            "ENSR must be measured, never asserted to be 100%",
        )
        self.assertIn("m_emergencyForwarded", body)


if __name__ == "__main__":
    unittest.main()
