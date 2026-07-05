#pragma once

#include "maffe/common.hpp"
#include "util/tree_ops.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

namespace maffe::detail {

struct TreeIntervals {
    std::vector<std::array<int, 2>> children;
    std::vector<int> first;
    std::vector<int> last;
    std::vector<int> leaf_count;
};

struct CommonClusterNode {
    int tree0_node = -1;
    int leaf_count = 0;
    int parent_cluster = -1;
    std::vector<int> roots;
    std::vector<int> children;
};

struct CommonClusterForest {
    std::vector<TreeIntervals> intervals;
    std::vector<int> cluster_of_tree0_node;
    std::vector<CommonClusterNode> clusters;
    std::vector<int> top_children;
};

[[nodiscard]] inline std::uint64_t interval_key(const int first, const int last) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(first)) << 32) |
        static_cast<std::uint32_t>(last);
}

[[nodiscard]] inline std::vector<int> tree0_leaf_order(
    const Tree& tree,
    const std::vector<std::array<int, 2>>& children
) {
    std::vector<int> order_of_leaf(tree.leaves(), -1);
    int next = 0;
    const auto dfs = [&](this auto&& self, const int node) -> void {
        if (node < 0)
            return;
        if (node < tree.leaves()) {
            order_of_leaf[node] = next++;
            return;
        }
        const auto [left, right] = children[node];
        self(left);
        self(right);
    };
    dfs(tree.root());
    return order_of_leaf;
}

[[nodiscard]] inline TreeIntervals build_tree_intervals(const Tree& tree, const std::vector<int>& order_of_leaf) {
    TreeIntervals intervals{
        .children = tree_children(tree, CutHandling::INCLUDE_CUT),
        .first = std::vector<int>(tree.vertices(), -1),
        .last = std::vector<int>(tree.vertices(), -1),
        .leaf_count = std::vector<int>(tree.vertices(), 0),
    };

    const auto dfs = [&](this auto&& self, const int node) -> void {
        if (node < 0)
            return;
        if (node < tree.leaves()) {
            intervals.first[node] = order_of_leaf[node];
            intervals.last[node] = order_of_leaf[node];
            intervals.leaf_count[node] = 1;
            return;
        }
        const auto [left, right] = intervals.children[node];
        self(left);
        self(right);
        if (left < 0 || intervals.leaf_count[left] == 0) {
            intervals.first[node] = right < 0 ? -1 : intervals.first[right];
            intervals.last[node] = right < 0 ? -1 : intervals.last[right];
            intervals.leaf_count[node] = right < 0 ? 0 : intervals.leaf_count[right];
            return;
        }
        if (right < 0 || intervals.leaf_count[right] == 0) {
            intervals.first[node] = intervals.first[left];
            intervals.last[node] = intervals.last[left];
            intervals.leaf_count[node] = intervals.leaf_count[left];
            return;
        }
        intervals.first[node] = std::min(intervals.first[left], intervals.first[right]);
        intervals.last[node] = std::max(intervals.last[left], intervals.last[right]);
        intervals.leaf_count[node] = intervals.leaf_count[left] + intervals.leaf_count[right];
    };
    dfs(tree.root());
    return intervals;
}

[[nodiscard]] inline CommonClusterForest build_common_cluster_forest(const AnnotatedInstance& instance) {
    const int tree_count = static_cast<int>(instance.trees.size());
    const int leaves = instance.trees.front().leaves();
    const int vertices = instance.trees.front().vertices();

    CommonClusterForest forest;
    forest.intervals.reserve(tree_count);
    const auto tree0_children = tree_children(instance.trees.front(), CutHandling::INCLUDE_CUT);
    const auto order_of_leaf = tree0_leaf_order(instance.trees.front(), tree0_children);
    for (const auto& tree : instance.trees)
        forest.intervals.push_back(build_tree_intervals(tree, order_of_leaf));

    std::unordered_map<std::uint64_t, int> tree0_node_of_interval;
    tree0_node_of_interval.reserve(vertices);
    std::vector roots_by_node(vertices, std::vector<int>(tree_count, -1));
    for (int node = instance.trees.front().leaves(); node < vertices; ++node) {
        roots_by_node[node][0] = node;
        tree0_node_of_interval.emplace(
            interval_key(forest.intervals[0].first[node], forest.intervals[0].last[node]),
            node
        );
    }

    for (int tree = 1; tree < tree_count; ++tree) {
        for (int node = instance.trees[tree].leaves(); node < instance.trees[tree].vertices(); ++node) {
            const int first = forest.intervals[tree].first[node];
            const int last = forest.intervals[tree].last[node];
            const int leaf_count = forest.intervals[tree].leaf_count[node];
            if (last - first + 1 != leaf_count)
                continue;
            const auto it = tree0_node_of_interval.find(interval_key(first, last));
            if (it == tree0_node_of_interval.end())
                continue;
            roots_by_node[it->second][tree] = node;
        }
    }

    forest.cluster_of_tree0_node.assign(vertices, -1);
    for (int node = instance.trees.front().leaves(); node < vertices; ++node) {
        const int leaf_count = forest.intervals[0].leaf_count[node];
        if (leaf_count < 2 || leaf_count >= leaves)
            continue;
        if (std::ranges::find(roots_by_node[node], -1) != roots_by_node[node].end())
            continue;
        forest.cluster_of_tree0_node[node] = static_cast<int>(forest.clusters.size());
        forest.clusters.push_back(CommonClusterNode{
            .tree0_node = node,
            .leaf_count = leaf_count,
            .roots = std::move(roots_by_node[node]),
            .children = {},
        });
    }

    // NOLINTBEGIN(bugprone-easily-swappable-parameters)
    const auto link = [&](this auto&& self, const int node, const int current_cluster) -> void {
        if (node < 0)
            return;
        int next_cluster = current_cluster;
        const int cluster_id = forest.cluster_of_tree0_node[node];
        if (cluster_id >= 0) {
            if (current_cluster >= 0) {
                forest.clusters[current_cluster].children.push_back(cluster_id);
                forest.clusters[cluster_id].parent_cluster = current_cluster;
            } else {
                forest.top_children.push_back(cluster_id);
            }
            next_cluster = cluster_id;
        }
        if (node < instance.trees.front().leaves())
            return;
        const auto [left, right] = forest.intervals[0].children[node];
        self(left, next_cluster);
        self(right, next_cluster);
    };
    // NOLINTEND(bugprone-easily-swappable-parameters)
    link(instance.trees.front().root(), -1);
    return forest;
}

} // namespace maffe::detail
