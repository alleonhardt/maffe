#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace maffe {

struct RootSetPackingColumnView {
    int column_id = -1;
    std::span<const int> leaves;
    std::span<const int> row_indices;
    std::span<const int> forced_rows;
    double lp_value = 0.0;
    double dual_cost = 0.0;
    double history_value = 0.0;
    bool warm_start = false;
    std::uint64_t tie_seed = 0;
};

struct RootSetPackingSolution {
    std::vector<int> columns;
    int saving = 0;
};

RootSetPackingSolution solve_root_set_packing_heuristic(
    int leaf_count,
    int row_count,
    int forced_row_count,
    int column_count,
    std::span<const RootSetPackingColumnView> columns
);

} // namespace maffe
