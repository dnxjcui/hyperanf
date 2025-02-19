from hyperanf.hyperanf import HyperANF


def create_small_test_graph():
    """Creates a small test graph."""
    return {
        0: {1, 2},
        1: {0, 3},
        2: {0, 3},
        3: {1, 2, 4},
        4: {3}
    }


# def create_large_test_graph():


def test_small_graph():
    """Test HyperANF on a small graph."""
    graph = create_small_test_graph()
    result = HyperANF(graph, precision=10)

    print(result)

    # Check that results are reasonable (approximate values)
    assert result[0] >= 3
    assert result[3] >= 5
    assert result[4] >= 2


def test_disconnected_graph():
    """Test HyperANF on a disconnected graph."""
    graph = {
        0: {1},
        1: {0},
        2: {3},
        3: {2}
    }
    result = HyperANF(graph, precision=10)

    # Each component should have its own small neighborhood
    assert result[0] >= 2
    assert result[2] >= 2


def test_single_node():
    """Test HyperANF on a single-node graph."""
    graph = {0: set()}
    result = HyperANF(graph, precision=10)
    assert result[0] == 1  # A single node should have itself in its neighborhood

