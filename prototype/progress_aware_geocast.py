"""Isolated prototype for progress-aware geocast relay selection.

This module is intentionally not imported by the ns-3 scenario. It is a small
reference model used to test the proposed algorithm before promotion.
"""
from dataclasses import dataclass
from math import dist


@dataclass(frozen=True)
class Node:
    node_id: int
    position: tuple[float, float, float]


@dataclass(frozen=True)
class ForwardDecision:
    node_id: int | None
    reason: str
    progress: float


class ProgressAwareGeocast:
    def __init__(self, sink: Node, radio_range: float, epsilon: float = 1e-9):
        self.sink = sink
        self.radio_range = radio_range
        self.epsilon = epsilon

    def choose_relay(self, current: Node, neighbors: list[Node]) -> ForwardDecision:
        current_distance = dist(current.position, self.sink.position)
        candidates = []
        for neighbor in neighbors:
            if neighbor.node_id == current.node_id:
                continue
            neighbor_distance = dist(neighbor.position, self.sink.position)
            link_distance = dist(current.position, neighbor.position)
            progress = current_distance - neighbor_distance
            if progress > self.epsilon and link_distance <= self.radio_range:
                candidates.append((progress, neighbor_distance, neighbor.node_id))

        if not candidates:
            return ForwardDecision(None, "no-positive-progress-relay", 0.0)

        # Maximize progress; break ties by choosing the node closest to sink.
        progress, _, node_id = max(candidates, key=lambda item: (item[0], -item[1]))
        return ForwardDecision(node_id, "positive-progress-relay", progress)


def run_path(nodes: list[Node], sink: Node, radio_range: float) -> tuple[bool, list[int]]:
    by_id = {node.node_id: node for node in nodes}
    current = nodes[0]
    path = [current.node_id]
    selector = ProgressAwareGeocast(sink, radio_range)
    for _ in range(len(nodes)):
        if current.node_id == sink.node_id:
            return True, path
        neighbors = [node for node in nodes if dist(current.position, node.position) <= radio_range]
        decision = selector.choose_relay(current, neighbors)
        if decision.node_id is None:
            return False, path
        current = by_id[decision.node_id]
        if current.node_id in path:
            return False, path + [current.node_id]
        path.append(current.node_id)
    return current.node_id == sink.node_id, path
