#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace maffe {

enum class EdgeState : std::uint8_t;
struct AnnotatedInstance;

struct LagrangianResult {
    std::vector<std::vector<int>> leaf_partition;
    std::vector<std::vector<int>> candidate_blocks;
    double lower_bound = 0.0;

    [[nodiscard]] const std::vector<std::vector<int>>& pricing_blocks() const {
        return candidate_blocks.empty() ? leaf_partition : candidate_blocks;
    }
};

class Lagrangian {
public:
    explicit Lagrangian(const AnnotatedInstance& instance);
    ~Lagrangian();
    Lagrangian(Lagrangian&&) noexcept;
    Lagrangian& operator=(Lagrangian&&) noexcept;
    Lagrangian(const Lagrangian&) = delete;
    Lagrangian& operator=(const Lagrangian&) = delete;

    LagrangianResult solve(
        const std::vector<std::vector<double>>& vertex_duals,
        const std::vector<std::vector<double>>& edge_duals,
        const std::vector<std::vector<EdgeState>>& edge_states
    );
    LagrangianResult solve_two_tree_orientation(
        int first_tree,
        int second_tree,
        const std::vector<std::vector<double>>& vertex_duals,
        const std::vector<std::vector<double>>& edge_duals,
        const std::vector<std::vector<EdgeState>>& edge_states
    );

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace maffe
