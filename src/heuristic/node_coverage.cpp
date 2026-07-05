#include "heuristic/node_coverage.hpp"

#include "util/tree_ops.hpp"

#include <algorithm>
#include <cstddef>

namespace maffe::heuristic {

void compute_node_coverage(
    const Tree& tree,
    const std::vector<std::vector<int>>& partition,
    std::vector<int>& cover
) {
    if (cover.size() < static_cast<std::size_t>(tree.vertices()))
        cover.resize(tree.vertices(), 0);
    std::fill(cover.begin(), cover.end(), 0);

    const auto children = detail::tree_children(tree, detail::CutHandling::EXCLUDE_CUT);
    const auto cut_component_root = [&](int node) {
        while (tree.parent[node] >= 0 && tree.edge_state[node] != EdgeState::CUT)
            node = tree.parent[node];
        return node;
    };

    for (const auto& component : partition) {
        if (component.size() < 2)
            continue;

        int lca = cut_component_root(component.front());
        bool same_cut_component = true;
        for (const int leaf : component) {
            if (cut_component_root(leaf) != lca) {
                same_cut_component = false;
                break;
            }
        }
        if (!same_cut_component)
            continue;

        std::vector<int> leaf_count(tree.vertices(), 0);
        for (const int leaf : component)
            leaf_count[leaf] = 1;

        std::vector<int> order;
        std::vector<int> stack = {lca};
        while (!stack.empty()) {
            const int node = stack.back();
            stack.pop_back();
            order.push_back(node);
            const auto [left, right] = children[node];
            if (left >= 0)
                stack.push_back(left);
            if (right >= 0)
                stack.push_back(right);
        }
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            const int node = *it;
            if (node < tree.leaves())
                continue;
            const auto [left, right] = children[node];
            const int left_count = left >= 0 ? leaf_count[left] : 0;
            const int right_count = right >= 0 ? leaf_count[right] : 0;
            leaf_count[node] = left_count + right_count;
        }

        while (leaf_count[lca] == static_cast<int>(component.size())) {
            const auto [left, right] = children[lca];
            if (left >= 0 && leaf_count[left] == static_cast<int>(component.size())) {
                lca = left;
            } else if (right >= 0 && leaf_count[right] == static_cast<int>(component.size())) {
                lca = right;
            } else {
                break;
            }
        }

        const auto mark = [&](this auto&& self, const int node) -> void {
            if (node < 0 || leaf_count[node] == 0)
                return;
            if (node >= tree.leaves())
                ++cover[node];
            const auto [left, right] = children[node];
            self(left);
            self(right);
        };
        mark(lca);
    }
}

} // namespace maffe::heuristic
