import pathlib
import unittest


ROOT = pathlib.Path(__file__).parents[1]
SCENARIO = ROOT / "ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc"
ROUTING = ROOT / "ns-allinone-3.29/ns-3.29/src/astro-fanet/model/astro-routing-protocol.cc"


class GeocastForwardingTests(unittest.TestCase):
    def test_emergency_packets_use_broadcast_destination(self):
        text = SCENARIO.read_text()
        self.assertIn("dataHdr.GetIsBroadcast ()", text)
        self.assertIn("Ipv4Address::GetBroadcast ()", text)

    def test_a3d_forward_decision_invokes_real_rebroadcast(self):
        text = ROUTING.read_text()
        decision = text.index("AstroAction bcastDecision")
        suppress = text.index("ACTION_SUPPRESS", decision)
        rebroadcast = text.index("BroadcastPacket", suppress)
        self.assertLess(suppress, rebroadcast)

    def test_rebroadcast_preserves_udp_and_updates_ast_header(self):
        text = ROUTING.read_text()
        self.assertIn("UdpHeader udpHeader", text)
        self.assertIn("RemoveHeader (udpHeader)", text)
        self.assertIn("AddHeader (udpHeader)", text)
        self.assertIn("SetPreviousRelayPos", text)

    def test_sink_pdr_counts_unique_origin_sequence_pairs(self):
        text = SCENARIO.read_text()
        self.assertIn("std::set<std::pair<uint32_t, uint32_t>>", text)
        self.assertIn("dataHdr.GetSequenceNumber ()", text)
        self.assertIn("alreadyDelivered", text)

    def test_progress_aware_guard_runs_before_rebroadcast(self):
        text = ROUTING.read_text()
        self.assertIn("IsProgressingRelay", text)
        decision = text.index("AstroAction bcastDecision")
        rebroadcast = text.index("BroadcastPacket (p->Copy (), header)", decision)
        guard = text.index("IsProgressingRelay", decision)
        self.assertLess(guard, rebroadcast)

    def test_first_hop_emergency_relay_diversity_is_bounded(self):
        text = ROUTING.read_text()
        self.assertIn("ShouldUseTrustAwareFallback", text)
        self.assertIn("GetHopCount ()", text)
        self.assertIn("<= 2", text)
        guard = text.index("if (!IsProgressingRelay")
        fallback = text.index("ShouldUseTrustAwareFallback", guard)
        self.assertLess(fallback, guard + 500)


if __name__ == "__main__":
    unittest.main()
