#include "branchandprice/master/root_master.hpp"
#include "maffe/common.hpp"
#include "util/tree_ops.hpp"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace maffe {
namespace {

[[nodiscard]] RootMasterLayout::TreeIndex build_tree_index(
    const Tree& tree,
    std::span<const std::array<int, 2>> children
) {
    const int vertices = tree.vertices();
    int levels = 1;
    while ((1 << levels) <= vertices)
        ++levels;

    RootMasterLayout::TreeIndex index{
        .parent = std::vector<int>(vertices, -1),
        .tin = std::vector<int>(vertices, -1),
        .tout = std::vector<int>(vertices, -1),
        .depth = std::vector<int>(vertices, 0),
        .component = std::vector<int>(vertices, -1),
        .up = std::vector<std::vector<int>>(levels, std::vector<int>(vertices, -1)),
    };
    for (int u = 0; u < vertices; ++u) {
        if (tree.parent[u] >= 0 && tree.edge_state[u] != EdgeState::CUT)
            index.parent[u] = tree.parent[u];
    }

    int timer = 0;
    const auto dfs = [&](this auto&& self, const int u, const int root) -> void {
        index.component[u] = root;
        index.tin[u] = timer++;
        index.up[0][u] = index.parent[u] < 0 ? u : index.parent[u];
        for (int level = 1; level < levels; ++level)
            index.up[level][u] = index.up[level - 1][index.up[level - 1][u]];

        const auto [left, right] = children[u];
        if (left >= 0) {
            index.depth[left] = index.depth[u] + 1;
            self(left, root);
        }
        if (right >= 0) {
            index.depth[right] = index.depth[u] + 1;
            self(right, root);
        }
        index.tout[u] = timer;
    };

    for (int u = 0; u < vertices; ++u) {
        if (index.parent[u] >= 0)
            continue;
        index.depth[u] = 0;
        dfs(u, u);
    }
    return index;
}

[[nodiscard]] bool is_ancestor(
    const RootMasterLayout::TreeIndex& index,
    const int ancestor,
    const int node
) {
    return index.component[ancestor] == index.component[node] &&
        index.tin[ancestor] <= index.tin[node] &&
        index.tout[node] <= index.tout[ancestor];
}

[[nodiscard]] int lca(
    const RootMasterLayout::TreeIndex& index,
    const int a,
    const int b
) {
    if (index.component[a] != index.component[b])
        return -1;
    if (is_ancestor(index, a, b))
        return a;
    if (is_ancestor(index, b, a))
        return b;

    int u = a;
    for (int level = static_cast<int>(index.up.size()) - 1; level >= 0; --level) {
        const int jump = index.up[level][u];
        if (!is_ancestor(index, jump, b))
            u = jump;
    }
    return index.up[0][u];
}

[[nodiscard]] std::vector<int> checked_leaves(const int leaf_count, std::span<const int> leaves) {
    if (leaves.empty())
        throw std::invalid_argument("column must contain at least one leaf");

    std::vector<int> checked(leaves.begin(), leaves.end());
    std::ranges::sort(checked);
    int previous = -1;
    for (const int leaf : checked) {
        if (leaf < 0 || leaf >= leaf_count)
            throw std::invalid_argument("column leaf out of range");
        if (leaf == previous)
            throw std::invalid_argument("column leaf set must be duplicate-free");
        previous = leaf;
    }
    return checked;
}

struct UsedSupport {
    std::vector<int> vertices;
    std::vector<int> edges;
};

[[nodiscard]] UsedSupport used_support(
    const Tree& tree,
    const RootMasterLayout::TreeIndex& index,
    std::span<const int> leaves
) {
    if (leaves.size() < 2)
        return {};

    std::vector<int> ordered(leaves.begin(), leaves.end());
    std::ranges::sort(ordered, [&](const int lhs, const int rhs) {
        return std::pair{index.tin[lhs], lhs} < std::pair{index.tin[rhs], rhs};
    });

    const int component = index.component[ordered.front()];
    if (component < 0)
        throw std::invalid_argument("column leaf set is disconnected by cuts");
    for (const int leaf : ordered) {
        if (index.component[leaf] != component)
            throw std::invalid_argument("column leaf set is disconnected by cuts");
    }

    std::vector<int> virtual_nodes = ordered;
    virtual_nodes.reserve(ordered.size() * 2);
    for (std::size_t i = 1; i < ordered.size(); ++i) {
        const int ancestor = lca(index, ordered[i - 1], ordered[i]);
        if (ancestor < 0)
            throw std::invalid_argument("column leaf set is disconnected by cuts");
        virtual_nodes.push_back(ancestor);
    }
    std::ranges::sort(virtual_nodes, [&](const int lhs, const int rhs) {
        return std::pair{index.tin[lhs], lhs} < std::pair{index.tin[rhs], rhs};
    });
    virtual_nodes.erase(std::unique(virtual_nodes.begin(), virtual_nodes.end()), virtual_nodes.end());

    std::vector<std::pair<int, int>> virtual_edges;
    virtual_edges.reserve(virtual_nodes.size() > 0 ? virtual_nodes.size() - 1 : 0);
    std::vector<int> stack;
    stack.reserve(virtual_nodes.size());
    stack.push_back(virtual_nodes.front());
    for (std::size_t i = 1; i < virtual_nodes.size(); ++i) {
        const int node = virtual_nodes[i];
        while (!stack.empty() && !is_ancestor(index, stack.back(), node))
            stack.pop_back();
        if (stack.empty())
            throw std::runtime_error("virtual Steiner tree construction failed");
        virtual_edges.emplace_back(stack.back(), node);
        stack.push_back(node);
    }

    UsedSupport support;
    for (const auto& [parent, child] : virtual_edges) {
        for (int node = child; node != parent; node = tree.parent[node]) {
            if (node < 0 || tree.parent[node] < 0 || tree.edge_state[node] == EdgeState::CUT)
                throw std::invalid_argument("column leaf set is disconnected by cuts");

            support.edges.push_back(node);
            const int ancestor = tree.parent[node];
            if (ancestor >= tree.leaves())
                support.vertices.push_back(ancestor);
        }
    }

    std::ranges::sort(support.edges);
    std::ranges::sort(support.vertices);
    support.vertices.erase(std::unique(support.vertices.begin(), support.vertices.end()), support.vertices.end());
    return support;
}

} // namespace

RootMasterLayout build_root_master_layout(const AnnotatedInstance& instance) {
    if (instance.trees.empty())
        throw std::invalid_argument("root master requires at least one tree");

    RootMasterLayout layout{
        .leaf_count = instance.trees.front().leaves(),
        .vertex_row_count = 0,
        .row_count = 0,
        .row_of_vertex = std::vector<std::vector<int>>(instance.trees.size()),
        .children = {},
        .tree_index = {},
    };
    layout.children.reserve(instance.trees.size());
    layout.tree_index.reserve(instance.trees.size());

    for (int i = 0; i < static_cast<int>(instance.trees.size()); ++i) {
        const auto& tree = instance.trees[i];
        if (tree.leaves() != layout.leaf_count)
            throw std::invalid_argument("all trees must have the same number of leaves");

        layout.children.push_back(detail::tree_children(
            tree,
            detail::CutHandling::EXCLUDE_CUT,
            detail::BinaryValidation::REQUIRE_BINARY
        ));
        layout.tree_index.push_back(build_tree_index(tree, layout.children.back()));
        auto& rows = layout.row_of_vertex[i];
        rows.resize(tree.vertices(), -1);
        for (int u = tree.leaves(); u < tree.vertices(); ++u)
            rows[u] = layout.row_count++;
    }
    layout.vertex_row_count = layout.row_count;
    return layout;
}

RootMasterColumn build_root_master_column(
    const AnnotatedInstance& instance,
    const RootMasterLayout& layout,
    std::span<const int> leaves
) {
    if (static_cast<int>(instance.trees.size()) != static_cast<int>(layout.row_of_vertex.size()) ||
        static_cast<int>(instance.trees.size()) != static_cast<int>(layout.children.size())) {
        throw std::invalid_argument("root master layout does not match instance");
    }

    RootMasterColumn column{
        .leaves = checked_leaves(layout.leaf_count, leaves),
        .row_indices = {},
        .used_vertices = std::vector<std::vector<int>>(instance.trees.size()),
        .used_edges = std::vector<std::vector<int>>(instance.trees.size()),
        .objective = 0.0,
    };

    column.objective = 1.0 - static_cast<double>(column.leaves.size());
    if (column.leaves.size() < 2)
        return column;

    std::size_t nnz = 0;
    for (int i = 0; i < static_cast<int>(instance.trees.size()); ++i) {
        const auto support = used_support(instance.trees[i], layout.tree_index[i], column.leaves);
        column.used_vertices[i] = support.vertices;
        column.used_edges[i] = support.edges;
        nnz += column.used_vertices[i].size();
    }

    column.row_indices.reserve(nnz);
    for (int i = 0; i < static_cast<int>(instance.trees.size()); ++i) {
        for (const int u : column.used_vertices[i])
            column.row_indices.push_back(layout.row_of_vertex[i][u]);
    }
    return column;
}

double root_master_reduced_cost(
    const RootMasterLayout& layout,
    const RootMasterColumn& column,
    std::span<const double> flat_row_duals
) {
    (void)layout;
    double reduced_cost = column.objective;
    for (const int row : column.row_indices)
        reduced_cost += flat_row_duals[row];
    return reduced_cost;
}

std::vector<std::vector<double>> zero_vertex_duals(const AnnotatedInstance& instance) {
    std::vector<std::vector<double>> duals(instance.trees.size());
    for (int i = 0; i < static_cast<int>(instance.trees.size()); ++i)
        duals[i].assign(instance.trees[i].vertices(), 0.0);
    return duals;
}

bool root_master_column_uses_tree_edge(const RootMasterColumn& column, const int tree, const int edge) {
    if (tree < 0 || tree >= static_cast<int>(column.used_edges.size()))
        return false;
    return std::binary_search(column.used_edges[tree].begin(), column.used_edges[tree].end(), edge);
}

} // namespace maffe
