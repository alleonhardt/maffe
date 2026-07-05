#pragma once

#include <cstdint>
#include <vector>

namespace maffe {

enum class EdgeState : std::uint8_t {
    UNKNOWN,
    FORCED,
    CUT,
};

struct Tree {
    std::vector<int> parent;
    std::vector<EdgeState> edge_state;
    [[nodiscard]] int vertices() const {
        return static_cast<int>(parent.size());
    }
    [[nodiscard]] int leaves() const {
        return vertices() / 2 + 1;
    }
    [[nodiscard]] int root() const {
        return vertices() - 1;
    }
};

struct AnnotatedInstance {
    std::vector<Tree> trees;
};

struct Result {
    std::vector<std::vector<int>> partition;
    bool feasible = true;
};

} // namespace maffe
