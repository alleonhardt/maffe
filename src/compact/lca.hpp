#pragma once

#include "maffe/common.hpp"

#include <array>
#include <bit>
#include <stdexcept>
#include <utility>
#include <vector>

namespace maffe::compact {

class LcaHelper {
public:
    explicit LcaHelper(const Tree& tree)
        : parent_(tree.parent),
          leaves_(tree.leaves()),
          children_(tree.vertices(), {-1, -1}),
          depth_(tree.vertices()),
          tin_(tree.vertices()),
          tout_(tree.vertices()) {
        for (int u = 0; u < tree.vertices(); ++u) {
            const int p = parent_[u];
            if (p < 0)
                continue;

            auto& [left, right] = children_[p];
            if (left < 0)
                left = u;
            else
                right = u;
        }

        levels_.assign(std::bit_width(static_cast<unsigned>(tree.vertices())), std::vector<int>(tree.vertices(), -1));
        for (int u = 0; u < tree.vertices(); ++u)
            levels_[0][u] = parent_[u];

        int timer = 0;
        std::vector<std::pair<int, bool>> stack;
        stack.emplace_back(tree.root(), false);
        while (!stack.empty()) {
            const auto [u, exit] = stack.back();
            stack.pop_back();
            if (exit) {
                tout_[u] = timer;
                continue;
            }

            tin_[u] = timer++;
            stack.emplace_back(u, true);
            auto [left, right] = children_[u];
            if (right >= 0) {
                depth_[right] = depth_[u] + 1;
                stack.emplace_back(right, false);
            }
            if (left >= 0) {
                depth_[left] = depth_[u] + 1;
                stack.emplace_back(left, false);
            }
        }

        for (int level = 1; level < static_cast<int>(levels_.size()); ++level) {
            for (int u = 0; u < tree.vertices(); ++u) {
                const int mid = levels_[level - 1][u];
                levels_[level][u] = mid < 0 ? -1 : levels_[level - 1][mid];
            }
        }
    }

    [[nodiscard]] int leaves() const {
        return leaves_;
    }

    [[nodiscard]] int parent(const int u) const {
        return parent_[u];
    }

    [[nodiscard]] bool ancestor(const int a, const int b) const {
        return tin_[a] <= tin_[b] && tout_[b] <= tout_[a];
    }

    [[nodiscard]] int lca(int a, int b) const {
        if (ancestor(a, b))
            return a;
        if (ancestor(b, a))
            return b;

        if (depth_[a] < depth_[b])
            std::swap(a, b);

        int diff = depth_[a] - depth_[b];
        for (int level = 0; diff != 0; ++level, diff >>= 1) {
            if ((diff & 1) != 0)
                a = levels_[level][a];
        }
        if (a == b)
            return a;

        for (int level = static_cast<int>(levels_.size()) - 1; level >= 0; --level) {
            if (levels_[level][a] == levels_[level][b])
                continue;
            a = levels_[level][a];
            b = levels_[level][b];
        }

        if (parent_[a] < 0)
            throw std::runtime_error("invalid tree for LCA");
        return parent_[a];
    }

private:
    std::vector<int> parent_;
    int leaves_ = 0;
    std::vector<std::array<int, 2>> children_;
    std::vector<int> depth_;
    std::vector<int> tin_;
    std::vector<int> tout_;
    std::vector<std::vector<int>> levels_;
};

} // namespace maffe::compact
