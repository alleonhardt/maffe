#pragma once

#include "maffe/common.hpp"

#include <vector>

namespace maffe::heuristic {

void compute_node_coverage(
    const Tree& tree,
    const std::vector<std::vector<int>>& partition,
    std::vector<int>& cover
);

} // namespace maffe::heuristic
