"""
Implementation of the HyperANF algorithm for approximate neighborhood function calculation,
as described by algorithm 2 in the paper "HyperANF: Approximating the Neighborhood Function
of Very Large Graphs on a Budget" by Palmer et al. (2002). https://arxiv.org/abs/1011.5599

This implementation uses the HyperLogLog algorithm for cardinality estimation, which is
implemented in the HLL package.
"""

from HLL import HyperLogLog
import copy


def HyperANF(graph, precision=10):
    """
    Implements the HyperANF algorithm for approximate neighborhood function calculation.
    Returns N(x, t), where N is the approximate neighborhood function at distance t. """
    # Initialize HyperLogLog counters for each node
    c = {v: HyperLogLog(precision, seed=42) for v in graph}

    # Add each node to its own counter
    for v in graph:
        c[v].add(str(v))  # Ensure consistent string representation

    t = 0
    NFs = []
    node_pairs = []
    denom = len(c) * (len(c) - 1) / 2
    avg_graph_distance = 0
    while True:
        s = sum(c[v].cardinality() for v in graph)  # Use .cardinality() for estimated cardinality

        NFs.append(s)
        node_pairs.append(NFs[-1] - NFs[-2] if len(NFs) > 1 else NFs[-1])

        changed = False
        new_c = copy.deepcopy(c)    # Create new instances for copying

        # Update each node's counter by merging neighbors
        for v in graph:
            m = new_c[v]
            for w in graph[v]:
                m.merge(c[w])

            if c[v].cardinality() != m.cardinality():  # Check cardinality difference
                changed = True

        c = new_c  # Update counters
        t += 1

        if not changed:
            break  # Stop when no counter changes

    # returns the avg graph distance
    avg_graph_distance = sum([(i + 1) * node_pairs[i] / sum(node_pairs) for i in range(len(node_pairs))])
    return avg_graph_distance
