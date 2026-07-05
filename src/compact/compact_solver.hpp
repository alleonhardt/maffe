#pragma once

#include "maffe.hpp"
#include "maffe/common.hpp"

#include <optional>

namespace maffe {

Result solve_with_compact_root_lp_seeded(
    const AnnotatedInstance& instance,
    LogLevel log_level = LogLevel::QUIET,
    std::optional<double> time_limit_seconds = std::nullopt,
    bool allow_abort_with_incumbent = false,
    int objective_offset = 0
);
Result solve_with_compact_gurobi(
    const AnnotatedInstance& instance,
    LogLevel log_level = LogLevel::QUIET,
    std::optional<double> time_limit_seconds = std::nullopt,
    bool allow_abort_with_incumbent = false,
    int objective_offset = 0
);

} // namespace maffe
