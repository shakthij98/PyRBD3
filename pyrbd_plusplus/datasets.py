import os
import pickle as pkl
import pandas as pd
import ast
from itertools import combinations
from tqdm import tqdm
import numpy as np

from .algorithms.sets import minimalcuts_optimized, minimalpaths
from .utils import relabel_boolexpr_to_str, sdp_boolexpr_to_str, sdp_boolexpr_length


import pyrbd_plusplus._core.pyrbd_plusplus_cpp as cpp


# Load data from a pickle file
def read_graph(directory, top, file_path=None):
    if file_path:
        with open(file_path, "rb") as handle:
            f = pkl.load(handle)
    else:
        with open(os.path.join(directory, "Pickle_" + top + ".pickle"), "rb") as handle:
            f = pkl.load(handle)
    G = f[0]
    pos = f[1]
    lable = f[2]

    return G, pos, lable


# Read mincutset from a csv file
def read_mincutset(directory, top):
    """Read mincutset and lengths from a csv file.

    Args:
        directory (str): The directory containing the CSV file.
        top (str): The name of the topology.

    Returns:
        tuple: A tuple containing two lists - the mincutsets and their lengths.
    """
    # Read the mincutset from a csv file
    df = pd.read_csv(os.path.join(directory, "Mincutset_" + top + ".csv"))

    # Convert the 'mincutsets' column from string representation to actual list
    df["mincutsets"] = df["mincutsets"].apply(ast.literal_eval)

    # Extract the 'mincutsets' column as a list
    mincutsets = df["mincutsets"].values.tolist()

    # Extract the 'length' column as a list
    lengths = df["length"].values.tolist()

    return mincutsets, lengths


# Read pathset from a csv file
def read_pathset(directory, top):
    """Read pathset and lengths from a csv file.

    Args:
        directory (str): The directory containing the CSV file.
        top (str): The name of the topology.

    Returns:
        tuple: A tuple containing two lists - the pathsets and their lengths.
    """
    # Read the pathset from a csv file
    df = pd.read_csv(os.path.join(directory, "Pathset_" + top + ".csv"))

    # Convert the 'pathsets' column from string representation to actual list
    df["pathsets"] = df["pathsets"].apply(ast.literal_eval)

    # Extract the 'pathsets' column as a list
    pathsets = df["pathsets"].values.tolist()

    # Extract the 'length' column as a list
    lengths = df["length"].values.tolist()

    return pathsets, lengths


# Evaluate the mincutset and save it to a csv file
def save_mincutset(directory, top):
    # Read the graph from the pickle file
    G, _, _ = read_graph(directory, top)

    # Get all node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Initialize a list to store mincutsets data
    mincutsets_data = []

    # Iterate through each pair of nodes and find the minimal cut sets
    for src, dst in tqdm(node_pairs, desc=f"Saving Mincutsets for {top}", leave=False):
        mincutset = minimalcuts_optimized(G, src, dst)
        if mincutset:
            mincutsets_data.append(
                {
                    "source": src,
                    "target": dst,
                    "mincutsets": repr(mincutset),
                    "length": len(mincutset),
                }
            )

    # Save mincutsets to CSV
    mincutsets_df = pd.DataFrame(mincutsets_data)
    mincutsets_df.to_csv(os.path.join(directory, f"Mincutset_{top}.csv"), index=False)


# Evaluate the pathset and save it to a csv file
def save_pathset(directory, top):
    # Read the graph from the pickle file
    G, _, _ = read_graph(directory, top)

    # Get all node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Initialize a list to store pathsets data
    pathsets_data = []

    # Iterate through each pair of nodes and find the simple paths
    for src, dst in tqdm(node_pairs, desc=f"Saving Pathsets for {top}", leave=False):
        pathset = minimalpaths(G, src, dst)
        if pathset:
            # Sort paths by length and for the same length, sort by lexicographical order
            pathset = sorted(pathset, key=lambda x: (len(x), x))
            pathsets_data.append(
                {
                    "source": src,
                    "target": dst,
                    "pathsets": repr(pathset),
                    "length": len(pathset),
                }
            )

    # Save pathsets to CSV
    pathsets_df = pd.DataFrame(pathsets_data)
    pathsets_df.to_csv(os.path.join(directory, f"Pathset_{top}.csv"), index=False)


# Evaluate the boolean expression from the mincutsets and save it to a csv file
def save_boolean_expression_from_mincutset(directory, top):
    # Read the mincutset from the csv file
    mincutsets, _ = read_mincutset(directory, top)

    # Read the graph from the pickle file
    G, _, _ = read_graph(directory, top)

    # Create node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Initialize a list to store boolean expressions data
    boolean_expressions_data = []

    # Iterate through each mincutset and evaluate the boolean expression
    for i, (src, dst) in tqdm(
        enumerate(node_pairs),
        desc=f"Saving Boolean Expressions from MCS for {top}",
        leave=False,
        total=len(node_pairs),
    ):
        # Relabel mincutset by adding 1 to ensure no 0 in the set
        mincutsets[i] = [[node + 1 for node in cutset] for cutset in mincutsets[i]]

        # Build the boolean expression from the mincutset
        expression = cpp.mcs.to_probaset(src, dst, mincutsets[i])

        # Append the boolean expression data
        boolean_expressions_data.append(
            {
                "source": src,
                "target": dst,
                "boolean_expression": relabel_boolexpr_to_str(expression),
                "length": len(expression),
            }
        )

    # Save boolean expressions to CSV
    boolean_expressions_df = pd.DataFrame(boolean_expressions_data)
    boolean_expressions_df.to_csv(
        os.path.join(directory, f"BoolExprMCS_{top}.csv"), index=False
    )


# Evaluate the boolean expression from the pathset and save it to a csv file
def save_boolean_expression_from_pathset(directory, top):
    # Read the pathset from the csv file
    pathsets, _ = read_pathset(directory, top)

    # Read the graph from the pickle file
    G, _, _ = read_graph(directory, top)

    # Create node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Initialize a list to store boolean expressions data
    boolean_expressions_data = []

    # Iterate through each pathset and evaluate the boolean expression
    for i, (src, dst) in tqdm(
        enumerate(node_pairs),
        desc=f"Saving Boolean Expressions from Pathset for {top}",
        leave=False,
        total=len(node_pairs),
    ):
        # Relabel pathset by adding 1 to ensure no 0 in the set
        pathsets[i] = [[node + 1 for node in path] for path in pathsets[i]]

        # Build the boolean expression from the pathset
        expression = cpp.pathset.to_probaset(src, dst, pathsets[i])

        # Append the boolean expression data
        boolean_expressions_data.append(
            {
                "source": src,
                "target": dst,
                "boolean_expression": relabel_boolexpr_to_str(expression),
                "length": len(expression),
            }
        )

    # Save boolean expressions to CSV
    boolean_expressions_df = pd.DataFrame(boolean_expressions_data)
    boolean_expressions_df.to_csv(
        os.path.join(directory, f"BoolExprPS_{top}.csv"), index=False
    )


# Evaluate the boolean expression from the SDP and save it to a csv file
def save_boolean_expression_from_sdp(directory, top):
    # Read the mincutset from the csv file
    pathsets, _ = read_pathset(directory, top)

    # Read the graph from the pickle file
    G, _, _ = read_graph(directory, top)

    # Create node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Initialize a list to store boolean expressions data
    boolean_expressions_data = []

    for i, (src, dst) in tqdm(
        enumerate(node_pairs),
        desc=f"Saving Boolean Expressions from SDP for {top}",
        leave=False,
        total=len(node_pairs),
    ):
        # Relabel pathset by adding 1 to ensure no 0 in the set
        pathsets[i] = [[node + 1 for node in path] for path in pathsets[i]]

        # Build the SDP set from the pathset
        expression = cpp.sdp.to_sdp_set_parallel(src, dst, pathsets[i])

        # Convert the SDP set to a string representation and calculate the expression length
        bool_expr_str = sdp_boolexpr_to_str(expression)

        # Append the result to the list
        boolean_expressions_data.append(
            {
                "src": src,
                "dst": dst,
                "boolean_expression": bool_expr_str,
                "length": sdp_boolexpr_length(expression),
            }
        )

    # Save the boolean expressions to a CSV file
    boolean_expressions_df = pd.DataFrame(boolean_expressions_data)
    boolean_expressions_df.to_csv(
        os.path.join(directory, f"BoolExprSDP_{top}.csv"), index=False
    )


# Evaluate the availability of a network based on the given parameters
def save_availability(directory, top):
    from .evaluator import evaluate_availability

    """
    Save the availability of the topology to a CSV file.
    The result is with all nodes availablity set to 0.9, 0.99, 0.999 and 0.9999.
    Args:
        directory (str): The directory where the topology is located.
        top (str): The name of the topology.
    """
    # Read the graph from the pickle file
    G, _, _ = read_graph(directory, top)

    # Create node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Create nodes probabilities dictionary
    node_proba = {
        "0.9": [0.9] * len(G.nodes()),
        "0.99": [0.99] * len(G.nodes()),
        "0.999": [0.999] * len(G.nodes()),
        "0.9999": [0.9999] * len(G.nodes()),
    }

    # Initialize a list to store availability data
    result_data = {
        "source": [pair[0] for pair in node_pairs],
        "target": [pair[1] for pair in node_pairs],
    }

    # Iterate through each probability and evaluate availability
    for prob, values in tqdm(
        node_proba.items(), desc=f"Saving Availability for {top}", leave=False
    ):
        nodes_probabilities = dict(zip(G.nodes(), values))

        # Evaluate availability for all pairs using SDP algorithm
        eval_result = evaluate_availability(
            G, nodes_probabilities, algorithm="sdp", parallel=True
        )
        avail = [t[2] for t in eval_result]

        # Save the results for this probability
        result_data[f"{prob} Availability"] = avail

    # Save availability data to CSV
    availability_df = pd.DataFrame(result_data)
    availability_df.to_csv(
        os.path.join(directory, f"Availability_{top}.csv"),
        index=False,
        float_format="%.9f",
    )


def evaluate_boolean_expressions(directory, topologies, node_prob):
    # Read the graph
    G, _, _ = read_graph(directory, topologies)

    # Check if all nodes has probabilities assigned
    if not all(node in node_prob for node in G.nodes()):
        raise ValueError("Not all nodes have probabilities assigned.")

    # Read the boolean expressions
    df = pd.read_csv(os.path.join(directory, f"BoolExprSDP_{topologies}.csv"))

    # Create node pairs
    node_pairs = list(combinations(G.nodes(), 2))

    # Evaluate availability for each node pair
    for i, (src, dst) in tqdm(enumerate(node_pairs), desc=f"Evaluating Boolean Expressions for {topologies}", leave=False):
        # Get the boolean expression for the current node pair
        bool_exprs = df.iloc[i]["boolean_expression"]
        print(bool_exprs)


def dataset_preparation(directory, topologies):
    """
    Prepare the dataset by saving pathsets, mincutsets, and boolean expressions.
    Args:
        directory (str): The directory where the datasets will be saved.
        topologies (list): A list of topology names to process.
    """
    for top in topologies:
        directory = f"topologies/{top}"
        save_pathset(directory, top)
        save_boolean_expression_from_pathset(directory, top)
        save_boolean_expression_from_sdp(directory, top)
        save_mincutset(directory, top)
        save_boolean_expression_from_mincutset(directory, top)
