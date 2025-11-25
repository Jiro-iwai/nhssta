#!/usr/bin/env python3
"""
Visualize .bench circuit files using NetworkX + Matplotlib
"""

import argparse
import re
import sys
from collections import defaultdict, deque

try:
    import networkx as nx
    import matplotlib.pyplot as plt
except ImportError:
    print("Error: networkx and matplotlib are required.")
    print("To install: pip install networkx matplotlib")
    sys.exit(1)


class CircuitParser:
    def __init__(self, bench_file):
        self.inputs = set()
        self.outputs = set()
        self.dffs = {}  # output -> input
        self.gates = {}  # output -> (gate_type, inputs)
        self.all_nodes = set()
        
        self.parse_file(bench_file)
    
    def parse_file(self, filename):
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                # INPUT
                m = re.match(r'INPUT\((\w+)\)', line)
                if m:
                    self.inputs.add(m.group(1))
                    self.all_nodes.add(m.group(1))
                    continue
                
                # OUTPUT
                m = re.match(r'OUTPUT\((\w+)\)', line)
                if m:
                    self.outputs.add(m.group(1))
                    self.all_nodes.add(m.group(1))
                    continue
                
                # DFF
                m = re.match(r'(\w+)\s*=\s*DFF\((\w+)\)', line)
                if m:
                    output, input_sig = m.group(1), m.group(2)
                    self.dffs[output] = input_sig
                    self.all_nodes.add(output)
                    self.all_nodes.add(input_sig)
                    continue
                
                # Gate: G123 = GATE_TYPE(G1, G2, ...)
                m = re.match(r'(\w+)\s*=\s*(\w+)\((.*)\)', line)
                if m:
                    output = m.group(1)
                    gate_type = m.group(2)
                    inputs_str = m.group(3)
                    inputs = [inp.strip() for inp in inputs_str.split(',')]
                    
                    self.gates[output] = (gate_type, inputs)
                    self.all_nodes.add(output)
                    for inp in inputs:
                        self.all_nodes.add(inp)
    
    def get_gate_type(self, node):
        """Get gate type for a node"""
        if node in self.inputs:
            return 'INPUT'
        if node in self.outputs:
            return 'OUTPUT'
        if node in self.dffs:
            return 'DFF'
        if node in self.gates:
            return self.gates[node][0]
        return 'UNKNOWN'
    
    def get_inputs(self, node):
        """Get input signals for a node"""
        if node in self.dffs:
            return [self.dffs[node]]
        if node in self.gates:
            return self.gates[node][1]
        return []
    
    def build_reverse_graph(self):
        """Build reverse graph: signal -> gates that use it"""
        reverse = defaultdict(list)
        for output, (gate_type, inputs) in self.gates.items():
            for inp in inputs:
                reverse[inp].append(output)
        for output, input_sig in self.dffs.items():
            reverse[input_sig].append(output)
        return reverse


class CriticalPathData:
    """Container for a single critical path parsed from nhssta output."""

    def __init__(self, index, delay):
        self.index = index
        self.delay = delay
        self.nodes = []


class NhsstaOutputParser:
    """Parse nhssta CLI output for critical path information."""

    PATH_HEADER_RE = re.compile(r'# Path (\d+)\s+\(delay:\s+([0-9.eE+-]+)\)')

    def __init__(self, filepath):
        self.filepath = filepath
        self.paths = []
        self._parse_file()

    def _parse_file(self):
        current_path = None
        reading_nodes = False

        try:
            with open(self.filepath, 'r') as f:
                for raw_line in f:
                    line = raw_line.strip()
                    if not line:
                        continue

                    header = self.PATH_HEADER_RE.match(line)
                    if header:
                        if current_path:
                            self.paths.append(current_path)
                        index = int(header.group(1))
                        delay = float(header.group(2))
                        current_path = CriticalPathData(index, delay)
                        reading_nodes = False
                        continue

                    if current_path is None:
                        continue

                    if line.startswith('#node'):
                        continue

                    if line.startswith('#-'):
                        # Toggle reading nodes between separator lines
                        reading_nodes = not reading_nodes
                        if not reading_nodes:
                            self.paths.append(current_path)
                            current_path = None
                        continue

                    if reading_nodes:
                        parts = line.split()
                        if parts:
                            node_name = parts[0]
                            current_path.nodes.append(node_name)

            if current_path and current_path not in self.paths:
                self.paths.append(current_path)

        except FileNotFoundError:
            print(f"Error: nhssta output file '{self.filepath}' not found")
            sys.exit(1)

    def get_paths(self):
        return self.paths


NODE_COLORS = {
    'INPUT': 'lightblue',
    'OUTPUT': 'lightgreen',
    'DFF': 'lightyellow',
    'AND': 'lightcoral',
    'OR': 'lightcyan',
    'NOT': 'lightgray',
    'INV': 'lightgray',
    'NAND': 'lightpink',
    'NOR': 'lightsalmon',
}


def label_for_node(node, node_type):
    if node_type == 'DFF':
        return f'{node}\n(DFF)'
    if node_type in ('INPUT', 'OUTPUT') or node_type == 'UNKNOWN':
        return node
    return f'{node}\n({node_type})'


def node_attributes(node, circuit, highlight_nodes):
    node_type = circuit.get_gate_type(node)
    attrs = {
        'fillcolor': NODE_COLORS.get(node_type, 'white'),
    }
    if node in highlight_nodes:
        attrs['fillcolor'] = '#ffe0e0'
    return attrs


def build_highlight_sets(paths):
    highlight_nodes = set()
    highlight_edges = set()
    normalized_paths = []

    for path_nodes in paths:
        if not path_nodes:
            continue
        normalized_paths.append(path_nodes)
        highlight_nodes.update(path_nodes)
        for i in range(len(path_nodes) - 1):
            highlight_edges.add((path_nodes[i], path_nodes[i + 1]))

    return normalized_paths, highlight_nodes, highlight_edges


def load_paths_from_file(filepath):
    parser = NhsstaOutputParser(filepath)
    paths = [path.nodes for path in parser.get_paths() if path.nodes]
    if not paths:
        print(f"Warning: no critical paths found in {filepath}")
    return paths


def compute_node_depths(circuit):
    """Compute approximate depth (distance from inputs) for each node."""
    memo = {}
    visiting = set()

    def dfs(node):
        if node in memo:
            return memo[node]
        if node in visiting:
            return 0
        visiting.add(node)
        inputs = circuit.get_inputs(node)
        if not inputs:
            depth = 0
        else:
            depth = max(dfs(inp) + 1 for inp in inputs)
        visiting.remove(node)
        memo[node] = depth
        return depth

    for node in circuit.all_nodes:
        dfs(node)
    return memo


def visualize_overview(circuit, output_file='circuit', highlight_paths=None):
    """Visualize circuit using NetworkX + Matplotlib."""
    highlight_paths = highlight_paths or []
    _, highlight_nodes, highlight_edges = build_highlight_sets(highlight_paths)

    G = nx.DiGraph()
    for node in circuit.all_nodes:
        G.add_node(node)

    for output, (_, inputs) in circuit.gates.items():
        for inp in inputs:
            G.add_edge(inp, output)
    for dff_out, dff_in in circuit.dffs.items():
        G.add_edge(dff_in, dff_out)

    depths = compute_node_depths(circuit)
    if depths:
        max_depth = max(depths.values())
    else:
        max_depth = 0

    # Force outputs to the rightmost layer
    for output in circuit.outputs:
        depths[output] = max_depth + 1
    max_depth = max(depths.values()) if depths else 0

    pos = {}
    # Use larger unit spacing to separate nodes
    x_unit = 5.0  # horizontal unit per depth layer
    y_unit = 3.0  # vertical unit per node in same layer

    # Group nodes by depth to distribute vertically
    # Separate DFFs to place them at the bottom
    depth_to_nodes = defaultdict(list)
    depth_to_dffs = defaultdict(list)
    for node, depth in depths.items():
        if circuit.get_gate_type(node) == 'DFF':
            depth_to_dffs[depth].append(node)
        else:
            depth_to_nodes[depth].append(node)

    # Find max nodes in any single layer for vertical sizing (excluding DFFs)
    all_depths = set(depth_to_nodes.keys()) | set(depth_to_dffs.keys())
    max_nodes_in_layer = max(
        (len(depth_to_nodes.get(d, [])) for d in all_depths), 
        default=1
    )
    max_dffs_in_layer = max(
        (len(depth_to_dffs.get(d, [])) for d in all_depths),
        default=0
    )

    # Node offset spacing to prevent nodes from aligning perfectly
    node_x_offset = 0.4  # x offset per node (larger)
    node_y_offset = 0.3  # y offset per node (larger)
    
    # Position regular nodes (inputs, gates, outputs) with small offsets
    node_counter = 0
    for depth, nodes in depth_to_nodes.items():
        nodes = sorted(nodes)
        count = len(nodes)
        for idx, node in enumerate(nodes):
            # Center nodes vertically within the layer
            centered_idx = idx - (count - 1) / 2.0
            # Add small unique offset to each node to prevent perfect alignment
            x_off = (node_counter % 5 - 2) * node_x_offset
            y_off = (node_counter % 3 - 1) * node_y_offset
            pos[node] = (depth * x_unit + x_off, -centered_idx * y_unit + y_off)
            node_counter += 1

    # Position DFFs at the bottom, below all other nodes
    # Each DFF gets a unique y coordinate to prevent sharing
    dff_x_offset = 1.5  # x offset for DFFs
    dff_y_spacing = y_unit * 2.0  # unique y spacing per DFF
    
    y_bottom = -max_nodes_in_layer * y_unit / 2 - y_unit * 3.0  # Start much further below regular nodes
    
    # Collect all DFFs and assign unique y positions
    all_dffs = []
    for depth, dffs in depth_to_dffs.items():
        for node in dffs:
            all_dffs.append((depth, node))
    
    # Sort DFFs by depth for consistent ordering
    all_dffs.sort(key=lambda x: (x[0], x[1]))
    
    for dff_idx, (depth, node) in enumerate(all_dffs):
        # Each DFF gets a unique y position - no sharing
        x_off = (dff_idx % 7 - 3) * dff_x_offset
        unique_y = y_bottom - dff_idx * dff_y_spacing
        pos[node] = (depth * x_unit + x_off, unique_y)
        node_counter += 1

    # Ensure every node has a position (covers isolated nodes)
    for idx, node in enumerate(sorted(G.nodes())):
        if node not in pos:
            pos[node] = (0, -idx * y_unit)

    # Calculate figure size proportional to coordinate range
    fig_width = max(20, (max_depth + 2) * x_unit * 0.5)
    total_vertical = max_nodes_in_layer + max_dffs_in_layer + 2
    fig_height = max(10, total_vertical * y_unit * 0.5)
    
    plt.figure(figsize=(fig_width, fig_height))
    ax = plt.gca()
    ax.set_title("Circuit Overview (NetworkX)")
    ax.axis("off")

    # Determine if we have highlighted paths
    has_highlights = len(highlight_nodes) > 0
    
    node_colors = []
    edgecolors = []
    node_alphas = []
    for node in G.nodes():
        attrs = node_attributes(node, circuit, highlight_nodes)
        if node in highlight_nodes:
            node_colors.append(attrs.get('fillcolor', 'white'))
            edgecolors.append('red')
            node_alphas.append(1.0)
        else:
            # Fade non-highlighted nodes when highlights exist
            if has_highlights:
                node_colors.append('#e0e0e0')  # light gray
                edgecolors.append('#cccccc')
                node_alphas.append(0.4)
            else:
                node_colors.append(attrs.get('fillcolor', 'white'))
                edgecolors.append('black')
                node_alphas.append(1.0)

    # Large fixed node size
    base_node_size = 1500
    
    # Get all y coordinates where nodes exist, grouped by x
    nodes_at_x = defaultdict(set)
    for node, (x, y) in pos.items():
        nodes_at_x[round(x / x_unit)].add(y)
    
    # Calculate node radius in data coordinates (approximate)
    node_radius = 0.15  # approximate radius in data units
    
    # For each edge, determine routing
    edges_by_source = defaultdict(list)
    edges_by_target = defaultdict(list)
    for u, v in G.edges():
        edges_by_source[u].append(v)
        edges_by_target[v].append(u)
    
    # Assign y-offsets for edges from the same source (output side)
    # Use larger spacing to prevent lines from bunching together
    wire_spacing = 0.25  # spacing between parallel wires
    
    source_y_offset = {}
    for u, targets in edges_by_source.items():
        targets_sorted = sorted(targets, key=lambda v: pos[v][1])
        n = len(targets_sorted)
        for idx, v in enumerate(targets_sorted):
            source_y_offset[(u, v)] = (idx - (n - 1) / 2) * wire_spacing
    
    # Assign y-offsets for edges to the same target (input side)
    target_y_offset = {}
    for v, sources in edges_by_target.items():
        sources_sorted = sorted(sources, key=lambda u: pos[u][1])
        n = len(sources_sorted)
        for idx, u in enumerate(sources_sorted):
            target_y_offset[(u, v)] = (idx - (n - 1) / 2) * wire_spacing
    
    # Assign unique global offset to each edge to prevent overlapping routes
    all_edges = list(G.edges())
    edge_global_offset = {}
    edge_x_offset = {}  # Unique x offset for vertical segments
    x_offset_spacing = 0.25  # spacing between vertical lines (larger)
    
    for idx, (u, v) in enumerate(all_edges):
        # Larger unique offset based on edge index for y
        edge_global_offset[(u, v)] = (idx % 10 - 5) * 0.15
        # Unique x offset so vertical segments don't share x coordinates
        edge_x_offset[(u, v)] = idx * x_offset_spacing
    
    for u, v in G.edges():
        x0, y0 = pos[u]
        x1, y1 = pos[v]
        is_highlighted = (u, v) in highlight_edges
        
        # Check if this edge goes INTO a DFF (D input)
        target_is_dff = circuit.get_gate_type(v) == 'DFF'
        
        if is_highlighted:
            color = 'red'
            lw = 2.0
            alpha = 1.0
            linestyle = '-'
        else:
            # Fade non-highlighted edges when highlights exist
            if has_highlights:
                color = '#cccccc'
                lw = 0.5
                alpha = 0.3
                linestyle = '-'
            else:
                color = 'black'
                lw = 1.0
                alpha = 0.7
                # Use dashed line for edges going INTO DFF (D input)
                if target_is_dff:
                    linestyle = '--'
                    color = '#0066cc'  # blue for DFF input
                else:
                    linestyle = '-'
        
        # Get y-offsets for this edge
        src_y_off = source_y_offset.get((u, v), 0)
        tgt_y_off = target_y_offset.get((u, v), 0)
        
        # Start from center of source node, end at center of target node
        x_start = x0  # center of source
        x_end = x1    # center of target
        y_start = y0  # center of source
        y_end = y1    # center of target
        
        # Check if nodes are horizontally adjacent (same y, adjacent x)
        depth0 = round(x0 / x_unit)
        depth1 = round(x1 / x_unit)
        
        if abs(y0 - y1) < 0.1 and abs(depth0 - depth1) <= 1:
            # Direct horizontal line for adjacent nodes (center to center)
            ax.annotate("", xy=(x_end, y_end), xytext=(x_start, y_start),
                        arrowprops=dict(arrowstyle='-|>', color=color, lw=lw, alpha=alpha,
                                       linestyle=linestyle))
        else:
            # 5-segment routing with diagonal only at node connections:
            # 1. Short diagonal from source center (divergence)
            # 2. Horizontal 
            # 3. Vertical (long-distance)
            # 4. Horizontal
            # 5. Short diagonal to target center (convergence)
            
            # Get global offset for this edge to separate from other edges
            global_off = edge_global_offset.get((u, v), 0)
            
            # Find node-free routing channel
            # Get all node y-positions between source and target x
            nodes_in_path = []
            for node, (node_x, node_y) in pos.items():
                if x_start < node_x < x_end and node != u and node != v:
                    nodes_in_path.append(node_y)
            
            # Calculate routing y that avoids nodes
            # Route above or below the path depending on direction
            node_margin = y_unit * 0.4  # margin around nodes
            
            if y_end > y_start:
                # Going down - route below all nodes in path
                if nodes_in_path:
                    route_channel_y = max(nodes_in_path) + node_margin + abs(global_off)
                else:
                    route_channel_y = max(y_start, y_end) + node_margin + abs(global_off)
            else:
                # Going up - route above all nodes in path
                if nodes_in_path:
                    route_channel_y = min(nodes_in_path) - node_margin - abs(global_off)
                else:
                    route_channel_y = min(y_start, y_end) - node_margin - abs(global_off)
            
            # Get unique x offset for this edge
            x_off = edge_x_offset.get((u, v), 0)
            
            # Divergence point: very short diagonal from source with unique x offset
            # Keep diagonal very short - just enough to show connection
            div_x = x_start + x_unit * 0.08 + x_off
            div_y = y_start + src_y_off * 0.3
            
            # Convergence point: very short diagonal to target with unique x offset
            conv_x = x_end - x_unit * 0.08 - x_off
            conv_y = y_end + tgt_y_off * 0.3
            
            # Horizontal routing y (in the channel that avoids nodes)
            horiz_y = route_channel_y + global_off * 0.5
            
            # Draw path as connected segments with linestyle
            # Segment 1: short diagonal from source center (divergence)
            ax.plot([x_start, div_x], [y_start, div_y], color=color, lw=lw, alpha=alpha, linestyle=linestyle)
            # Segment 2: vertical to routing channel (unique x position)
            ax.plot([div_x, div_x], [div_y, horiz_y], color=color, lw=lw, alpha=alpha, linestyle=linestyle)
            # Segment 3: horizontal along routing channel (avoiding nodes)
            ax.plot([div_x, conv_x], [horiz_y, horiz_y], color=color, lw=lw, alpha=alpha, linestyle=linestyle)
            # Segment 4: vertical from channel to convergence point (unique x position)
            ax.plot([conv_x, conv_x], [horiz_y, conv_y], color=color, lw=lw, alpha=alpha, linestyle=linestyle)
            # Segment 5: short diagonal to target center (convergence) with arrow
            ax.annotate("", xy=(x_end, y_end), xytext=(conv_x, conv_y),
                        arrowprops=dict(arrowstyle='-|>', color=color, lw=lw, alpha=alpha,
                                       linestyle=linestyle))
            
            # Draw small dots at connection points (corners)
            dot_size = 8
            dot_color = color if is_highlighted else 'black'
            dot_alpha = alpha
            ax.plot(div_x, div_y, 'o', color=dot_color, markersize=dot_size * 0.4, alpha=dot_alpha)
            ax.plot(div_x, horiz_y, 'o', color=dot_color, markersize=dot_size * 0.4, alpha=dot_alpha)
            ax.plot(conv_x, horiz_y, 'o', color=dot_color, markersize=dot_size * 0.4, alpha=dot_alpha)
            ax.plot(conv_x, conv_y, 'o', color=dot_color, markersize=dot_size * 0.4, alpha=dot_alpha)
    
    node_sizes = [base_node_size * 1.2 if node in circuit.outputs else base_node_size for node in G.nodes()]
    
    # Draw nodes with alpha based on highlight status
    nodes_list = list(G.nodes())
    for i, node in enumerate(nodes_list):
        nx.draw_networkx_nodes(G, pos, nodelist=[node], 
                               node_color=[node_colors[i]], 
                               edgecolors=[edgecolors[i]], 
                               node_size=[node_sizes[i]],
                               alpha=node_alphas[i])
    
    # Draw labels outside the nodes (above the node)
    # Calculate label offset based on node size
    label_offset = y_unit * 0.25  # offset above the node
    
    for i, node in enumerate(nodes_list):
        label = label_for_node(node, circuit.get_gate_type(node))
        x, y = pos[node]
        alpha = node_alphas[i]
        fontcolor = 'black' if alpha > 0.5 else '#888888'
        # Place label above the node
        ax.text(x, y + label_offset, label, fontsize=9, ha='center', va='bottom', 
                color=fontcolor, alpha=max(0.5, alpha))

    plt.tight_layout()
    plt.savefig(f'{output_file}.png', dpi=200)
    plt.close()
    print(f"Circuit visualization saved to {output_file}.png")


def visualize_extracted_paths(circuit, highlight_paths, output_file='critical_path'):
    """Visualize only the extracted critical paths"""
    _, highlight_nodes, highlight_edges = build_highlight_sets(highlight_paths)
    if not highlight_nodes:
        print("No critical paths to visualize.")
        return

    G = nx.DiGraph()
    for node in highlight_nodes:
        G.add_node(node)
    for src, dst in highlight_edges:
        G.add_edge(src, dst)

    # Use spring layout for extracted paths
    pos = nx.spring_layout(G, k=2, iterations=50)
    
    plt.figure(figsize=(12, 8))
    ax = plt.gca()
    ax.set_title("Critical Paths")
    ax.axis("off")

    node_colors = []
    for node in G.nodes():
        attrs = node_attributes(node, circuit, highlight_nodes)
        node_colors.append(attrs.get('fillcolor', 'white'))

    nx.draw_networkx_nodes(G, pos, node_color=node_colors, edgecolors='red', 
                           node_size=1500, alpha=1.0)
    nx.draw_networkx_edges(G, pos, edge_color='red', width=2.0, 
                           arrows=True, arrowstyle='-|>')
    
    labels = {node: label_for_node(node, circuit.get_gate_type(node)) for node in G.nodes()}
    nx.draw_networkx_labels(G, pos, labels, font_size=9)

    plt.tight_layout()
    plt.savefig(f'{output_file}.png', dpi=200)
    plt.close()
    print(f"Critical path graph saved to {output_file}.png")


def visualize_path_to_output(circuit, target_output, max_depth=10, output_file='circuit_path'):
    """Visualize path from inputs/DFFs to a specific output"""
    if target_output not in circuit.outputs:
        print(f"Error: {target_output} is not an output")
        return
    
    visited = set()
    queue = deque([(target_output, 0)])
    visited.add(target_output)
    nodes_info = {}
    edges = []
    
    # BFS to find all nodes in path
    while queue:
        node, depth = queue.popleft()
        if depth >= max_depth:
            continue
        
        gate_type = circuit.get_gate_type(node)
        nodes_info[node] = gate_type
        
        inputs = circuit.get_inputs(node)
        for inp in inputs:
            if inp not in visited:
                visited.add(inp)
                if depth + 1 < max_depth:
                    queue.append((inp, depth + 1))
            edges.append((inp, node, gate_type == 'DFF'))
    
    G = nx.DiGraph()
    for node in nodes_info:
        G.add_node(node)
    for from_node, to_node, is_dff in edges:
        if from_node in nodes_info:
            G.add_edge(from_node, to_node)
    
    pos = nx.spring_layout(G, k=2, iterations=50)
    
    plt.figure(figsize=(16, 10))
    ax = plt.gca()
    ax.set_title(f"Path to {target_output}")
    ax.axis("off")

    node_colors = []
    for node in G.nodes():
        gate_type = nodes_info.get(node, 'UNKNOWN')
        node_colors.append(NODE_COLORS.get(gate_type, 'white'))

    nx.draw_networkx_nodes(G, pos, node_color=node_colors, edgecolors='black', 
                           node_size=1500, alpha=1.0)
    
    # Draw edges with different styles for DFF connections
    dff_edges = [(u, v) for u, v, is_dff in edges if is_dff and u in nodes_info and v in nodes_info]
    normal_edges = [(u, v) for u, v, is_dff in edges if not is_dff and u in nodes_info and v in nodes_info]
    
    if normal_edges:
        nx.draw_networkx_edges(G, pos, edgelist=normal_edges, edge_color='black', 
                               width=1.0, arrows=True, arrowstyle='-|>')
    if dff_edges:
        nx.draw_networkx_edges(G, pos, edgelist=dff_edges, edge_color='red', 
                               width=1.0, arrows=True, arrowstyle='-|>', style='dashed')
    
    labels = {node: label_for_node(node, nodes_info.get(node, 'UNKNOWN')) for node in G.nodes()}
    nx.draw_networkx_labels(G, pos, labels, font_size=9)

    plt.tight_layout()
    plt.savefig(f'{output_file}.png', dpi=200)
    plt.close()
    print(f"Path visualization saved to {output_file}.png")


def visualize_statistics(circuit):
    """Print circuit statistics"""
    print("\n=== Circuit Statistics ===")
    print(f"Inputs: {len(circuit.inputs)}")
    print(f"Outputs: {len(circuit.outputs)}")
    print(f"DFFs: {len(circuit.dffs)}")
    print(f"Total gates: {len(circuit.gates)}")
    
    gate_types = defaultdict(int)
    for gate_type, _ in circuit.gates.values():
        gate_types[gate_type] += 1
    
    print("\nGate types:")
    for gate_type, count in sorted(gate_types.items()):
        print(f"  {gate_type}: {count}")
    
    print(f"\nTotal nodes: {len(circuit.all_nodes)}")


def main():
    parser = argparse.ArgumentParser(description="Visualize .bench circuits and nhssta critical paths")
    parser.add_argument('bench_file', help='.bench file to visualize')
    parser.add_argument('output_name', nargs='?', default='circuit', help='Base name for generated graphs')
    parser.add_argument('--path', dest='path_output', metavar='OUTPUT', help='Visualize path to specified output node')
    parser.add_argument('--highlight-path', dest='highlight_file', metavar='FILE', help='Highlight critical paths from nhssta output on the overview graph')
    parser.add_argument('--extract-path', dest='extract_file', metavar='FILE', help='Extract critical paths from nhssta output into a separate graph')
    parser.add_argument('--max-depth', dest='max_depth', type=int, default=10, help='Max depth when traversing --path (default: 10)')
    args = parser.parse_args()

    circuit = CircuitParser(args.bench_file)
    visualize_statistics(circuit)

    highlight_paths = load_paths_from_file(args.highlight_file) if args.highlight_file else []
    extract_paths = load_paths_from_file(args.extract_file) if args.extract_file else []

    rendered = False

    if extract_paths:
        visualize_extracted_paths(circuit, extract_paths, output_file=f'{args.output_name}_extract')
        rendered = True
    elif args.extract_file:
        print(f"No critical paths extracted from {args.extract_file}")

    if highlight_paths:
        visualize_overview(circuit, output_file=args.output_name, highlight_paths=highlight_paths)
        rendered = True
    elif args.highlight_file:
        print(f"No critical paths highlighted from {args.highlight_file}")

    if args.path_output:
        visualize_path_to_output(circuit, args.path_output, max_depth=args.max_depth, output_file=f'{args.output_name}_path')
        rendered = True

    if not rendered:
        visualize_overview(circuit, output_file=args.output_name)
        print("\nTip: Use --path OUTPUT to visualize path to a specific output")
        print(f"Example: python visualize_bench.py {args.bench_file} {args.output_name} --path G290")


if __name__ == '__main__':
    main()
