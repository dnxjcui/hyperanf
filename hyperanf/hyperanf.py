"""
Implementation of the HyperANF algorithm for approximate neighborhood function calculation,
as described by algorithm 2 in the paper "HyperANF: Approximating the Neighborhood Function
of Very Large Graphs on a Budget" by Palmer et al. (2002). https://arxiv.org/abs/1011.5599

This implementation uses the HyperLogLog algorithm for cardinality estimation, which is
implemented in the HLL package.
"""

from HLL import HyperLogLog
import copy


def union(hll1, hll2):
    """Merge two HyperLogLog counters and return a new instance."""
    new_hll = copy.deepcopy(hll1)
    new_hll.merge(hll2)
    return new_hll


def HyperANF(graph, precision=10):
    """Implements the HyperANF algorithm for approximate neighborhood function calculation."""
    # Initialize HyperLogLog counters for each node
    c = {v: HyperLogLog(precision) for v in graph}

    # Add each node to its own counter
    for v in graph:
        c[v].add(str(v))  # Ensure consistent string representation

    t = 0
    while True:
        s = sum(c[v].cardinality() for v in graph)  # Use .cardinality() for estimated cardinality
        print(f"Step {t}: Neighborhood size sum = {s}")

        changed = False
        new_c = {v: HyperLogLog(precision) for v in graph}  # Create new instances (?)

        # Update each node's counter by merging neighbors
        for v in graph:
            m = copy.deepcopy(c[v])
            for w in graph[v]:
                m = union(m, c[w])

            new_c[v] = m
            if c[v].cardinality() != m.cardinality():  # Check cardinality difference
                changed = True

        c = new_c  # Update counters
        t += 1

        if not changed:
            break  # Stop when no counter changes

    return {v: c[v].cardinality() for v in graph}  # Use .cardinality() for final estimates
