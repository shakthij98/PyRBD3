"""
PyRBD3 Minimal Demo
====================
This demo shows the bare‑minimum usage of the library:

1) Load a graph
2) Build a node-probability dict
3) Evaluate availability
   - single (src, dst)
   - full topology (all pairs)
   - sequential vs parallel

No custom layout, no colors, no extra utilities.
"""

import time
from pyrbd_plusplus import read_graph, evaluate_availability


def print_section_start(title: str):
    print("\n" + "=" * 70)
    print(title)

def print_section_end():
    print("=" * 70)

def main():
    # Load a sample graph
    print_section_start("Load graph")
    topo = "Germany_17"
    G, _, _ = read_graph(f"topologies/{topo}", topo)
    print(f"Loaded {topo}: |V|={len(G.nodes())}, |E|={len(G.edges())}")

    # Build node probability dict (uniform p=0.9 as example)
    node_prob = {node: 0.9 for node in G.nodes()}
    first_five = list(node_prob.items())[:5]
    print("Sample Node Probabilities (first 5):", {n: p for n, p in first_five})
    print_section_end()

    # Single pair evaluation with several algorithms
    src, dst = 0, 1
    print_section_start(f"Single pair availability: src={src}, dst={dst}")
    # PyRBD
    pyrbd_res = evaluate_availability(G, node_prob, src=src, dst=dst, algorithm="pyrbd")
    print("PyRBD Result:", pyrbd_res)
    # PyRBD++ MCS
    pyrbdpp_mcs_res = evaluate_availability(G, node_prob, src=src, dst=dst, algorithm="mcs")
    print("PyRBD++ MCS Result:", pyrbdpp_mcs_res)
    # PyRBD++ Pathset
    pyrbdpp_pathset_res = evaluate_availability(G, node_prob, src=src, dst=dst, algorithm="pathset")
    print("PyRBD++ Pathset Result:", pyrbdpp_pathset_res)
    # PyRBD3
    pyrbdpp_sdp_res = evaluate_availability(G, node_prob, src=src, dst=dst, algorithm="sdp")
    print("PyRBD3 Result:", pyrbdpp_sdp_res)
    print_section_end()
    
    # Full topology (all pairs), sequential
    print_section_start("All pairs (PyRBD3, sequential)")
    t0 = time.time()
    res_all = evaluate_availability(G, node_prob, algorithm="sdp")
    print("Sample Results (first 2 pairs):", res_all[:2])
    dt = time.time() - t0
    print(f"Time: {dt:.4f} s")
    print_section_end()

    # Full topology (all pairs), parallel
    print_section_start("All pairs (PyRBD3, parallel)")
    t0 = time.time()
    res_all_p = evaluate_availability(G, node_prob, algorithm="sdp", parallel=True)
    print("Sample Results (first 2 pairs):", res_all_p[:2])
    dt = time.time() - t0
    print(f"Time: {dt:.4f} s")
    print_section_end()
    print("Customize algorithms, src/dst, probabilities, or topology as needed.")


if __name__ == "__main__":
    main()
