#pragma once

#include "branchandprice/master/set_packing_heuristic.hpp"
#include "maffe.hpp"

#include <span>

namespace maffe {

RootSetPackingSolution solve_root_set_packing_scip(
    int leaf_count,
    int row_count,
    int forced_row_count,
    int column_count,
    std::span<const RootSetPackingColumnView> columns,
    std::span<const int> incumbent_columns,
    int objective_offset,
    double time_limit_seconds,
    int root_cut_rounds,
    bool root_only,
    LogLevel log_level = LogLevel::NORMAL
);

RootSetPackingSolution solve_root_set_packing_highs(
    int leaf_count,
    int row_count,
    int forced_row_count,
    int column_count,
    std::span<const RootSetPackingColumnView> columns,
    std::span<const int> incumbent_columns,
    int objective_offset,
    double time_limit_seconds,
    LogLevel log_level = LogLevel::NORMAL
);

} // namespace maffe
