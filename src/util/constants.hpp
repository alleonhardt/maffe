#pragma once

#include <array>
#include <cstddef>

namespace maffe::constants {

// Branch-and-price.
inline constexpr double scip_reduced_cost_tol = 1e-7;
inline constexpr double scip_bound_tol = 1e-6;
inline constexpr double scip_branch_tol = 1e-7;
inline constexpr int scip_pseudocost_reliable_samples = 8;
inline constexpr int scip_setppc_separation_frequency = -1;
inline constexpr int scip_separation_max_rounds = 0;
inline constexpr int scip_root_separation_max_rounds = 0;
inline constexpr int scip_verbose_display_frequency = 10;
inline constexpr double pricing_dual_snap_tol = 1e-12;
inline constexpr std::array<double, 3> scip_dual_stabilization_current_weights = {0.75, 0.50, 0.25};
inline constexpr double scip_dual_stabilization_update_weight = 0.25;
inline constexpr int scip_dual_stabilization_reset_rounds = 3;
inline constexpr double scip_dual_stabilization_min_gap = 1e-6;
inline constexpr int scip_root_pricing_lookahead_rounds = 2;
inline constexpr double scip_root_pricing_lookahead_step_scale = 0.5;
inline constexpr double scip_root_pricing_lookahead_max_step = 1.0;
inline constexpr double scip_root_pricing_lookahead_min_gap = 1e-6;
inline constexpr int scip_root_set_packing_heuristic_candidate_limit = 5000;
inline constexpr int scip_root_set_packing_heuristic_randomized_passes = 4;
inline constexpr int scip_root_set_packing_heuristic_improvement_passes = 2;
inline constexpr int scip_root_set_packing_heuristic_pair_bucket_size = 6;
inline constexpr int scip_root_set_packing_heuristic_ejection_candidate_limit = 128;
inline constexpr int scip_root_set_packing_heuristic_ejection_refill_limit = 768;
inline constexpr int scip_root_set_packing_heuristic_max_ejected = 4;
inline constexpr double scip_root_set_packing_heuristic_min_probability = 0.05;
inline constexpr double scip_root_set_packing_heuristic_dual_gain_weight = 0.25;
inline constexpr double scip_root_set_packing_heuristic_history_decay = 0.95;
inline constexpr double scip_root_set_packing_heuristic_history_increment = 0.10;
inline constexpr int scip_root_set_packing_heuristic_exact_seed_limit = 64;
inline constexpr int scip_root_set_packing_heuristic_exact_neighborhood_limit = 48;
inline constexpr int scip_set_packing_submip_min_pricing_calls = 8;
inline constexpr int scip_set_packing_submip_root_min_columns = 1500;
inline constexpr int scip_set_packing_submip_node_min_columns = 3000;
inline constexpr int scip_set_packing_submip_root_min_new_columns = 350;
inline constexpr int scip_set_packing_submip_node_min_new_columns = 1000;
inline constexpr int scip_set_packing_submip_max_root_calls = 8;
inline constexpr int scip_set_packing_submip_max_node_calls = 6;
inline constexpr double scip_set_packing_submip_root_seconds = 30.0;
inline constexpr double scip_set_packing_submip_node_seconds = 30.0;

// Heuristic column generation.
inline constexpr int heuristic_colgen_min_leaves = 1000;
inline constexpr double heuristic_colgen_lambda_bound = 1.0;
inline constexpr double heuristic_colgen_conicbundle_time_fraction = 0.15;
inline constexpr double heuristic_colgen_perturbation_time_fraction = 0.05;
inline constexpr int heuristic_colgen_conicbundle_max_model_size = 20;
inline constexpr int heuristic_colgen_conicbundle_max_bundle_size = 20;
inline constexpr int heuristic_colgen_conicbundle_max_iterations = 150;
inline constexpr int heuristic_colgen_max_set_packing_columns = 12000;
inline constexpr std::array<int, 3> heuristic_colgen_set_packing_stage_columns = {3000, 7000, 12000};
inline constexpr std::array<double, 3> heuristic_colgen_set_packing_stage_seconds = {10.0, 30.0, 0.0};
inline constexpr double heuristic_colgen_min_set_packing_seconds = 0.5;
inline constexpr double heuristic_colgen_perturbation_multiplier_min = 0.85;
inline constexpr double heuristic_colgen_perturbation_multiplier_max = 1.15;
inline constexpr int heuristic_colgen_set_packing_max_cut_rounds_root = 3;
inline constexpr int heuristic_colgen_set_packing_early_max_cut_rounds_root = 1;
inline constexpr int heuristic_colgen_set_packing_max_cut_rounds = 1;
inline constexpr int heuristic_colgen_set_packing_max_cut_stall_rounds_root = 1;
inline constexpr int heuristic_colgen_set_packing_max_cuts_root = 2000;
inline constexpr int heuristic_colgen_set_packing_max_cuts = 200;

// Root LP column generation.
inline constexpr double root_lp_reduced_cost_tol = 1e-9;
inline constexpr double root_lp_bound_tol = 1e-6;
inline constexpr double root_lp_basis_tol = 1e-8;
inline constexpr double root_lp_min_remaining_seconds = 1e-3;

// Lagrangian dynamic programming.
inline constexpr double lagrangian_dp_abs_tol = 1e-9;
inline constexpr double lagrangian_dp_rel_tol = 1e-12;

// Compact model.
inline constexpr double compact_capacity_tol = 1e-6;
inline constexpr double compact_seed_dual_tol = 1e-8;
inline constexpr double compact_seed_support_tol = 0.5;
inline constexpr int compact_capacity_separation_row_limit = 200;
inline constexpr int compact_capacity_root_separation_row_limit = 1000;
inline constexpr int compact_capacity_seed_row_limit = 500;
inline constexpr std::size_t compact_capacity_seed_nonzero_limit = 3'000'000;
inline constexpr int compact_scip_max_cuts = 1000;
inline constexpr int compact_scip_max_cuts_root = 5000;
inline constexpr int compact_scip_probing_max_prerounds = 0;
inline constexpr int compact_rectangle_pair_seed_max_degree = 256;
inline constexpr int compact_rectangle_pair_seed_min_sources = 3;
inline constexpr int compact_rectangle_pair_seed_candidate_limit = 50'000;

// Solver orchestration.
inline constexpr int small_exact_leaves = 10;
inline constexpr int verbose_residual_solve_leaves = 200;
inline constexpr int compact_subsolver_min_trees = 3;
inline constexpr int compact_subsolver_max_leaves = 500;
inline constexpr double residual_deadline_remaining_weight_factor = 2.0;
inline constexpr double residual_deadline_min_fraction = 0.01;
inline constexpr double residual_deadline_max_fraction = 0.40;
inline constexpr double residual_deadline_min_seconds = 0.25;
inline constexpr int presolve_header_repeat_rounds = 10;

// Reductions.
inline constexpr int small_cluster_leaf_threshold = 50;
inline constexpr int max_cluster_subinstance_leaves = 200;
inline constexpr int small_cut_aware_cluster_leaves = 50;
inline constexpr int sparse_completion_leaf_limit = 512;
inline constexpr int cut_aware_preferred_seed_side_limit = 512;
inline constexpr double competition_cut_aware_search_seconds = 60.0;
inline constexpr int competition_cut_aware_budget_check_interval = 256;
inline constexpr int timeout_check_interval = 64;

} // namespace maffe::constants
