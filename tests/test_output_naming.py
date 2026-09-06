import unittest
from pathlib import Path


SCENARIO = Path(__file__).parents[1] / "ns-allinone-3.29/ns-3.29/scratch/astro-fanet-sim.cc"


class OutputNamingTests(unittest.TestCase):
    def test_csv_output_name_separates_byzantine_fraction(self):
        source = SCENARIO.read_text()
        csv_block = source[source.index("// Write to CSV for batch analysis"):]
        self.assertIn("byzFraction", csv_block)
        self.assertIn("static_cast<int> (byzFraction * 100.0)", csv_block)

    def test_animation_output_name_separates_byzantine_fraction(self):
        source = SCENARIO.read_text()
        animation_block = source[source.index("if (enableAnim)"):source.index("// Write to CSV for batch analysis")]
        self.assertIn("static_cast<int> (byzFraction * 100.0)", animation_block)
    def test_simulation_validates_core_parameters_before_node_creation(self):
        source = SCENARIO.read_text()
        validation = source[source.index("cmd.Parse (argc, argv);"):source.index("// Set random seed")]
        for token in (
            "nUavs < 2",
            "simTime <= 0.0",
            "pktRate < 0.0",
            "maxSpeed < minSpeed",
            "byzFraction < 0.0 || byzFraction >= 1.0",
            "return 1;",
        ):
            self.assertIn(token, validation)

    def test_simulation_validates_protocol_and_mobility(self):
        source = SCENARIO.read_text()
        validation = source[source.index("cmd.Parse (argc, argv);"):source.index("// Set random seed")]
        self.assertIn('protocol != "astro"', validation)
        self.assertIn('mobility != "gm3d"', validation)


if __name__ == "__main__":
    unittest.main()
