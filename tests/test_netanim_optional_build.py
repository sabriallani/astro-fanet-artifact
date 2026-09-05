import unittest
from pathlib import Path


class NetAnimOptionalBuildTests(unittest.TestCase):
    def test_scenario_does_not_require_netanim_for_numeric_runs(self):
        source = (
            Path(__file__).resolve().parents[1]
            / "ns-allinone-3.29"
            / "ns-3.29"
            / "scratch"
            / "astro-fanet-sim.cc"
        ).read_text()
        self.assertIn("#ifdef ASTRO_ENABLE_NETANIM\n#include \"ns3/netanim-module.h\"", source)
        self.assertIn("#ifdef ASTRO_ENABLE_NETANIM\n  std::unique_ptr<AnimationInterface> anim;", source)
        self.assertIn("#endif  // ASTRO_ENABLE_NETANIM", source)


if __name__ == "__main__":
    unittest.main()
