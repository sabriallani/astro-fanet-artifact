import unittest

from prototype.progress_aware_geocast import Node, ProgressAwareGeocast, run_path


class ProgressAwareGeocastTests(unittest.TestCase):
    def setUp(self):
        self.sink = Node(99, (100.0, 0.0, 0.0))
        self.selector = ProgressAwareGeocast(self.sink, radio_range=35.0)

    def test_rejects_neighbors_that_do_not_progress(self):
        current = Node(2, (40.0, 0.0, 0.0))
        backward = Node(1, (20.0, 0.0, 0.0))
        forward = Node(3, (60.0, 0.0, 0.0))
        decision = self.selector.choose_relay(current, [backward, forward])
        self.assertEqual(decision.node_id, 3)
        self.assertGreater(decision.progress, 0.0)

    def test_returns_suppression_only_when_no_useful_relay_exists(self):
        current = Node(2, (40.0, 0.0, 0.0))
        backward = Node(1, (20.0, 0.0, 0.0))
        decision = self.selector.choose_relay(current, [backward])
        self.assertIsNone(decision.node_id)
        self.assertEqual(decision.reason, "no-positive-progress-relay")

    def test_line_topology_reaches_sink_without_loop(self):
        nodes = [
            Node(0, (0.0, 0.0, 0.0)),
            Node(1, (20.0, 0.0, 0.0)),
            Node(2, (40.0, 0.0, 0.0)),
            Node(3, (60.0, 0.0, 0.0)),
            Node(4, (80.0, 0.0, 0.0)),
            self.sink,
        ]
        delivered, path = run_path(nodes, self.sink, radio_range=35.0)
        self.assertTrue(delivered)
        self.assertEqual(path, [0, 1, 2, 3, 4, 99])
        self.assertEqual(len(path), len(set(path)))


if __name__ == "__main__":
    unittest.main()
