#pragma once

#include "maffe.hpp"
#include "maffe/common.hpp"

#include <chrono>

namespace maffe::heuristic {

[[nodiscard]] Result solve_colgen(
    const AnnotatedInstance& instance,
    std::chrono::steady_clock::time_point deadline,
    const Result* initial_solution,
    int objective_offset,
    LogLevel log_level
);

} // namespace maffe::heuristic
