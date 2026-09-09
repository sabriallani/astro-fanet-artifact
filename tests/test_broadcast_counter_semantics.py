"""Regression contract for the routing-level physical broadcast counter.

`m_totalBroadcasts` is exported as the run-level `broadcasts` metric.  It must
count an actual call to `BroadcastPacket()` exactly once, rather than both the
rebroadcast decision and the subsequent send path.  The dedicated
`m_rebroadcasts` counter remains the decision counter used by SP.
"""

from pathlib import Path
import unittest


SOURCE = (Path(__file__).parents[1]
          / "ns-allinone-3.29/ns-3.29/src/astro-fanet/model/astro-routing-protocol.cc")


class BroadcastCounterSemanticsTests(unittest.TestCase):
    def test_routing_broadcast_counter_is_incremented_only_at_send_boundary(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertEqual(
            source.count("m_totalBroadcasts++"), 1,
            "routing-level broadcasts must count exactly one actual send, not "
            "both the decision path and BroadcastPacket()",
        )

        start = source.find("AstroRoutingProtocol::BroadcastPacket")
        self.assertNotEqual(start, -1, "BroadcastPacket definition is required")
        body = source[start:start + 1600]
        self.assertIn("m_totalBroadcasts++", body)


if __name__ == "__main__":
    unittest.main()
