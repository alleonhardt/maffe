#pragma once

#include "maffe.hpp"
#include "maffe/common.hpp"

#include <optional>
#include <span>
#include <vector>

namespace maffe {

Result solve_with_scip_branch_and_price( // NOLINT(bugprone-easily-swappable-parameters)
    const AnnotatedInstance& instance,
    LogLevel log_level = LogLevel::NORMAL,
    std::span<const std::vector<int>> seed_columns = {},
    std::vector<std::vector<int>>* generated_columns = nullptr,
    const Result* initial_solution = nullptr,
    bool allow_abort_with_incumbent = false,
    int objective_offset = 0,
    std::optional<double> time_limit_seconds = std::nullopt,
    double acceptable_factor = 1.0,
    int acceptable_offset = 0
);

} // namespace maffe
