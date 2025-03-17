from hyperanf.hyperanf import HyperANF
import os

import sys
sys.path.append('src')

import hll_module

def create_small_test_graph():
    """Creates a small test graph."""
    return {
        0: {1, 2},
        1: {0, 3},
        2: {0, 3},
        3: {1, 2, 4},
        4: {3}
    }


def create_medium_test_graph():
    """Creates a medium test graph."""
    return {
        0: {1, 2},
        1: {0, 2, 3},
        2: {0, 1},
        3: {1, 4},
        4: {3}
    }


def create_large_test_graph():
    return {
        0: {1, 4, 9},
        1: {0, 2, 4},
        2: {1, 3},
        3: {2},
        4: {0, 1, 5},
        5: {4},
        6: {7},
        7: {6, 8},
        8: {7, 9},
        9: {0, 8}
    }


def cnr2000_graph():
    """ Taken from http://konect.cc/networks/dimacs10-cnr-2000/ """
    fp = os.path.join("test_hyperanf", "data", "cnr-2000.txt")

    G = {}

    counter = 0
    for line in open(fp, "r"):
        # skip first line
        if counter == 0:
            counter += 1
            d = line.split(" ")
            d = int(d[0])
            continue
        ln = line.split(" ")
        G[counter] = [int(x) for x in ln[:-1]]
        # G[counter] = line.split(" ") # our delimiter is space
        counter += 1

    assert (len(G) == d)

    # let's find the max degree & avg degree
    max_degree = 0
    degrees = []
    for k, v in G.items():
        degrees.append(len(v))
        if len(v) > max_degree:
            max_degree = len(v)

    avg_degree = sum(degrees) / len(degrees)
    print(f"Max degree: {max_degree}")
    print(f"Avg degree: {avg_degree}")

    result = HyperANF(G, precision=4)
    print(result)


def large_graph():
    """Test HyperANF on a large graph."""
    graph = create_large_test_graph()
    result = HyperANF(graph, precision=4)

    print(result)

    # Check that results are reasonable (approximate values)
    assert result >= 2


def medium_graph():
    """Test HyperANF on a medium graph."""
    graph = create_medium_test_graph()
    result = HyperANF(graph, precision=10)

    print(result)

    # Check that results are reasonable (approximate values)
    assert result >= 1.5
    # assert result[0] >= 3
    # assert result[3] >= 5
    # assert result[4] >= 2


def small_graph():
    """Test HyperANF on a small graph."""
    graph = create_small_test_graph()
    result = HyperANF(graph, precision=10)

    print(result)

    # Check that results are reasonable (approximate values)
    assert result >= 2
    # assert result[0] >= 3
    # assert result[3] >= 5
    # assert result[4] >= 2



def disconnected_graph():
    """Test HyperANF on a disconnected graph."""
    graph = {
        0: {1},
        1: {0},
        2: {3},
        3: {2}
    }
    result = HyperANF(graph, precision=10)

    print(result)

    assert result >= 1
    # Each component should have its own small neighborhood
    # assert result[0] >= 2
    # assert result[2] >= 2

#
# def test_single_node():
#     """Test HyperANF on a single-node graph."""
#     graph = {0: set()}
#     result = HyperANF(graph, precision=10)
#     assert result[0] == 1  # A single node should have itself in its neighborhood

if __name__ == "__main__":
    cnr2000_graph()