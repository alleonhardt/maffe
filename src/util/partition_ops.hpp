#pragma once

#include <algorithm>
#include <vector>

namespace maffe::detail {

[[nodiscard]] inline bool partition_block_less(const std::vector<int>& lhs, const std::vector<int>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

inline void sort_partition_blocks(std::vector<std::vector<int>>& partition) {
    for (auto& block : partition)
        std::ranges::sort(block);
    std::ranges::sort(partition, partition_block_less);
}

} // namespace maffe::detail
