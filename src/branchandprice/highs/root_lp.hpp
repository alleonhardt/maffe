#pragma once

#include "branchandprice/master/root_master.hpp"
#include "maffe.hpp"
#include "maffe/common.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace maffe {

enum class BasisStatus : std::uint8_t {
    LOWER,
    BASIC,
    UPPER,
    ZERO,
};

struct Basis {
    std::vector<BasisStatus> column_status;
    std::vector<BasisStatus> row_status;
};

struct RootLpWarmStart {
    std::vector<std::vector<int>> columns;
    std::vector<double> column_values;
    Basis basis;
};

struct RootLpResult {
    RootMasterLayout layout;
    RootLpWarmStart warm_start;
    std::optional<Result> heuristic_solution;
    std::vector<std::vector<double>> vertex_duals;
    std::vector<std::vector<double>> edge_duals;
    double objective = 0.0;
    double lagrangian_lower_bound = 0.0;
    double highs_seconds = 0.0;
    double pricing_seconds = 0.0;
    std::int64_t total_simplex_iterations = 0;
    int rounds = 0;
    bool interrupted = false;
};

RootLpResult solve_root_lp_with_highs(
    const AnnotatedInstance& instance,
    LogLevel log_level = LogLevel::NORMAL,
    std::span<const std::vector<int>> seed_columns = {},
    int objective_offset = 0,
    std::optional<double> time_limit_seconds = std::nullopt,
    bool allow_abort_with_incumbent = false
);

} // namespace maffe
