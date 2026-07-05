#pragma once

#include "maffe.hpp"
#include "maffe/common.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace maffe {

using Lift = std::function<Result(Result)>;

struct Reduced {
    AnnotatedInstance instance;
    Lift lift;
    int objective_offset = 0;
    int reduction_count = 0;
    int subinstance_count = 0;
    int largest_subinstance = 0;
    double subinstance_seconds = 0.0;
};

struct SubsolveTimeout : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class SolveContext {
public:
    explicit SolveContext(const SolveOptions& options = {});
    Result solve(AnnotatedInstance instance);
    Result solve_residual(
        AnnotatedInstance instance,
        const std::vector<std::vector<int>>* seed_columns = nullptr,
        std::vector<std::vector<int>>* generated_columns = nullptr,
        const Result* initial_solution = nullptr,
        int objective_offset = 0
    );
    void check_timeout() const;
    [[nodiscard]] LogLevel log_level() const { return log_level_; }
    [[nodiscard]] bool heuristic_mode() const { return heuristic_mode_; }

private:
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> residual_deadline(
        const AnnotatedInstance& instance
    ) const;
    [[nodiscard]] bool deadline_expired(
        std::optional<std::chrono::steady_clock::time_point> deadline
    ) const;
    [[nodiscard]] bool subsolve_deadline_expired(
        std::optional<std::chrono::steady_clock::time_point> deadline
    ) const;
    [[nodiscard]] std::optional<Result> try_heuristic_colgen(
        const AnnotatedInstance& instance,
        std::vector<Lift>& lifts,
        int objective_offset
    );

    std::optional<std::chrono::steady_clock::time_point> final_exact_deadline_;
    mutable int timeout_check_skip_ = 0;
    int active_problem_leaves_ = 0;
    double acceptable_factor_ = 1.0;
    int acceptable_offset_ = 0;
    LogLevel log_level_ = LogLevel::NORMAL;
    bool heuristic_mode_ = false;
};

[[nodiscard]] inline bool instance_has_at_most_leaves(const AnnotatedInstance& instance, const int max_leaves) {
    return !instance.trees.empty() && instance.trees.front().leaves() <= max_leaves;
}

std::optional<Reduced> try_cherry_picking(const AnnotatedInstance& instance);
std::optional<Reduced> try_chain_rule(const AnnotatedInstance& instance);
std::optional<Reduced> try_three_two_reduction(const AnnotatedInstance& instance);
std::optional<Reduced> try_five_three_reduction(const AnnotatedInstance& instance);
std::optional<Reduced> try_cluster_reduction(
    const AnnotatedInstance& instance,
    SolveContext& context,
    int objective_offset
);
std::optional<Reduced> try_cut_aware_cluster_reduction(
    const AnnotatedInstance& instance,
    SolveContext& context,
    int objective_offset
);
Result solve_annotated(
    AnnotatedInstance instance,
    std::optional<double> timeout_seconds = std::nullopt,
    double acceptable_factor = 1.0,
    int acceptable_offset = 0,
    LogLevel log_level = LogLevel::NORMAL,
    bool heuristic_mode = false
);

} // namespace maffe
