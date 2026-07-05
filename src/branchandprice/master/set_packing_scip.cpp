#include "branchandprice/master/set_packing_scip.hpp"

#include "util/constants.hpp"
#include "util/log.hpp"
#include "util/scip_error_log.hpp"

#include "Highs.h"

#include <algorithm>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define SCIP_ConshdlrData MaffeSetPackingScipConshdlrData
#define SCIP_ConsData MaffeSetPackingScipConsData
#define SCIP_EventhdlrData MaffeSetPackingScipEventhdlrData

#include "scip/cons_linear.h"
#include "scip/cons_setppc.h"
#include "scip/pub_message.h"
#include "scip/scip.h"
#include "scip/scip_param.h"
#include "scip/scip_prob.h"
#include "scip/scip_sol.h"
#include "scip/scip_solve.h"
#include "scip/scip_var.h"
#include "scip/scipdefplugins.h"

#undef SCIP_ConshdlrData
#undef SCIP_ConsData
#undef SCIP_EventhdlrData

namespace maffe {
namespace {

void check(const SCIP_RETCODE status) {
    if (status != SCIP_OKAY)
        throw std::runtime_error("SCIP set packing call failed");
}

void check(const HighsStatus status) {
    if (status != HighsStatus::kOk)
        throw std::runtime_error("HiGHS set packing call failed");
}

void check_highs_run(const HighsStatus status) {
    if (status == HighsStatus::kError)
        throw std::runtime_error("HiGHS set packing solve failed");
}

void log_highs_lines(const std::string_view message) {
    std::size_t begin = 0;
    while (begin < message.size()) {
        const std::size_t newline = message.find('\n', begin);
        const std::size_t end = newline == std::string_view::npos ? message.size() : newline;
        const std::size_t trimmed_end = end > begin && message[end - 1] == '\r' ? end - 1 : end;
        if (trimmed_end > begin)
            logging::line("set-packing-highs: ", message.substr(begin, trimmed_end - begin));
        if (newline == std::string_view::npos)
            break;
        begin = newline + 1;
    }
}

void highs_log_callback(
    const int callback_type,
    const std::string& message,
    const HighsCallbackOutput*,
    HighsCallbackInput*,
    void*
) {
    if (callback_type == kCallbackLogging)
        log_highs_lines(message);
}

struct ScipDeleter {
    void operator()(SCIP* scip) const noexcept {
        if (scip != nullptr)
            (void)SCIPfree(&scip);
    }
};

using scip_error_log::ignore_unused;

SCIP_DECL_MESSAGEWARNING(scip_message_warning) {
    ignore_unused(messagehdlr, file);
    scip_error_log::log_prefixed_lines("set-packing-scip: ", msg);
}

SCIP_DECL_MESSAGEDIALOG(scip_message_dialog) {
    ignore_unused(messagehdlr, file);
    scip_error_log::log_prefixed_lines("set-packing-scip: ", msg);
}

SCIP_DECL_MESSAGEINFO(scip_message_info) {
    ignore_unused(messagehdlr, file);
    scip_error_log::log_prefixed_lines("set-packing-scip: ", msg);
}

void install_message_handler(SCIP* scip, const bool enable_solver_output) {
    SCIP_MESSAGEHDLR* raw_messagehdlr = nullptr;
    check(SCIPmessagehdlrCreate(
        &raw_messagehdlr,
        TRUE,
        nullptr,
        !enable_solver_output,
        scip_message_warning,
        scip_message_dialog,
        scip_message_info,
        nullptr,
        nullptr
    ));
    check(SCIPsetMessagehdlr(scip, raw_messagehdlr));
    check(SCIPmessagehdlrRelease(&raw_messagehdlr));
}

void validate_column(
    const RootSetPackingColumnView& column,
    const int leaf_count,
    const int row_count,
    const int forced_row_count,
    const int column_count
) {
    if (column.column_id < 0 || column.column_id >= column_count)
        throw std::invalid_argument("SCIP set packing column id out of range");

    int previous_leaf = -1;
    for (const int leaf : column.leaves) {
        if (leaf < 0 || leaf >= leaf_count)
            throw std::invalid_argument("SCIP set packing column leaf out of range");
        if (leaf <= previous_leaf)
            throw std::invalid_argument("SCIP set packing column leaves must be sorted and unique");
        previous_leaf = leaf;
    }

    int previous_row = -1;
    for (const int row : column.row_indices) {
        if (row < 0 || row >= row_count)
            throw std::invalid_argument("SCIP set packing column row out of range");
        if (row <= previous_row)
            throw std::invalid_argument("SCIP set packing column rows must be sorted and unique");
        previous_row = row;
    }

    int previous_forced_row = -1;
    for (const int row : column.forced_rows) {
        if (row < 0 || row >= forced_row_count)
            throw std::invalid_argument("set packing column forced row out of range");
        if (row <= previous_forced_row)
            throw std::invalid_argument("set packing column forced rows must be sorted and unique");
        previous_forced_row = row;
    }
}

void apply_settings(
    SCIP* scip,
    const double time_limit_seconds,
    const int root_cut_rounds,
    const bool root_only,
    const bool enable_solver_output
) {
    check(SCIPsetBoolParam(scip, "misc/catchctrlc", FALSE));
    check(SCIPsetIntParam(scip, "display/verblevel", enable_solver_output ? 4 : 0));
    if (enable_solver_output)
        check(SCIPsetIntParam(scip, "display/freq", constants::scip_verbose_display_frequency));
    check(SCIPsetEmphasis(scip, SCIP_PARAMEMPHASIS_FEASIBILITY, TRUE));
    check(SCIPsetSeparating(scip, SCIP_PARAMSETTING_DEFAULT, TRUE));
    check(SCIPsetIntParam(
        scip,
        "separating/maxroundsroot",
        root_cut_rounds
    ));
    check(SCIPsetIntParam(
        scip,
        "separating/maxrounds",
        constants::heuristic_colgen_set_packing_max_cut_rounds
    ));
    check(SCIPsetIntParam(
        scip,
        "separating/maxstallroundsroot",
        constants::heuristic_colgen_set_packing_max_cut_stall_rounds_root
    ));
    check(SCIPsetIntParam(
        scip,
        "separating/maxcutsroot",
        constants::heuristic_colgen_set_packing_max_cuts_root
    ));
    check(SCIPsetIntParam(
        scip,
        "separating/maxcuts",
        constants::heuristic_colgen_set_packing_max_cuts
    ));
    check(SCIPsetIntParam(scip, "parallel/maxnthreads", 1));
    check(SCIPsetHeuristics(scip, SCIP_PARAMSETTING_AGGRESSIVE, TRUE));
    if (root_only) {
        check(SCIPsetLongintParam(scip, "limits/nodes", 1));
        check(SCIPsetLongintParam(scip, "limits/totalnodes", 1));
    }
    if (time_limit_seconds > 0.0)
        check(SCIPsetRealParam(scip, "limits/time", time_limit_seconds));
}

[[nodiscard]] bool install_incumbent(
    SCIP* scip,
    const std::vector<SCIP_VAR*>& vars,
    const std::span<const int> incumbent_columns
) {
    if (incumbent_columns.empty())
        return false;

    SCIP_SOL* sol = nullptr;
    check(SCIPcreateSol(scip, &sol, nullptr));
    try {
        for (SCIP_VAR* var : vars)
            check(SCIPsetSolVal(scip, sol, var, 0.0));
        for (const int column_id : incumbent_columns) {
            if (column_id < 0 || column_id >= static_cast<int>(vars.size()))
                throw std::invalid_argument("SCIP set packing incumbent column id out of range");
            check(SCIPsetSolVal(scip, sol, vars[static_cast<std::size_t>(column_id)], 1.0));
        }

        SCIP_Bool stored = FALSE;
        check(SCIPaddSolFree(scip, &sol, &stored));
        return stored == TRUE;
    } catch (...) {
        if (sol != nullptr)
            (void)SCIPfreeSol(scip, &sol);
        throw;
    }
}

} // namespace

RootSetPackingSolution solve_root_set_packing_scip(
    const int leaf_count,
    const int row_count,
    const int forced_row_count,
    const int column_count,
    const std::span<const RootSetPackingColumnView> columns,
    const std::span<const int> incumbent_columns,
    const int objective_offset,
    const double time_limit_seconds,
    const int root_cut_rounds,
    const bool root_only,
    const LogLevel log_level
) {
    if (leaf_count < 0 || row_count < 0 || forced_row_count < 0 || column_count < 0)
        throw std::invalid_argument("SCIP set packing dimensions must be nonnegative");
    if (time_limit_seconds < 0.0)
        throw std::invalid_argument("SCIP set packing time limit must be nonnegative");

    SCIP* raw_scip = nullptr;
    check(SCIPcreate(&raw_scip));
    std::unique_ptr<SCIP, ScipDeleter> scip(raw_scip);
    const bool enable_solver_output = logging::enabled(log_level);
    install_message_handler(scip.get(), enable_solver_output);
    check(SCIPincludeDefaultPlugins(scip.get()));
    apply_settings(scip.get(), time_limit_seconds, root_cut_rounds, root_only, enable_solver_output);
    check(SCIPcreateProbBasic(scip.get(), "heuristic-set-packing"));
    check(SCIPsetObjsense(scip.get(), SCIP_OBJSENSE_MINIMIZE));
    check(SCIPaddOrigObjoffset(scip.get(), static_cast<SCIP_Real>(leaf_count + objective_offset)));

    std::vector<SCIP_VAR*> vars(static_cast<std::size_t>(column_count), nullptr);
    std::vector<int> savings(static_cast<std::size_t>(column_count), 0);
    std::vector<std::vector<SCIP_VAR*>> row_vars(static_cast<std::size_t>(row_count));
    std::vector<std::vector<SCIP_VAR*>> forced_row_vars(static_cast<std::size_t>(forced_row_count));
    for (const RootSetPackingColumnView& column : columns) {
        validate_column(column, leaf_count, row_count, forced_row_count, column_count);
        if (column.leaves.size() < 2)
            continue;

        SCIP_VAR* var = nullptr;
        const std::string name = std::format("x_{}", column.column_id);
        const double saving = static_cast<double>(column.leaves.size()) - 1.0;
        savings[static_cast<std::size_t>(column.column_id)] = static_cast<int>(saving);
        check(SCIPcreateVarBasic(
            scip.get(),
            &var,
            name.c_str(),
            0.0,
            1.0,
            -saving,
            SCIP_VARTYPE_BINARY
        ));
        check(SCIPaddVar(scip.get(), var));
        vars[static_cast<std::size_t>(column.column_id)] = var;
        for (const int row : column.row_indices)
            row_vars[static_cast<std::size_t>(row)].push_back(var);
        for (const int row : column.forced_rows)
            forced_row_vars[static_cast<std::size_t>(row)].push_back(var);
        check(SCIPreleaseVar(scip.get(), &var));
    }

    for (int row = 0; row < row_count; ++row) {
        auto& vars_in_row = row_vars[static_cast<std::size_t>(row)];
        if (vars_in_row.empty())
            continue;
        SCIP_CONS* cons = nullptr;
        const std::string name = std::format("row_{}", row);
        check(SCIPcreateConsBasicSetpack(
            scip.get(),
            &cons,
            name.c_str(),
            static_cast<int>(vars_in_row.size()),
            vars_in_row.data()
        ));
        check(SCIPaddCons(scip.get(), cons));
        check(SCIPreleaseCons(scip.get(), &cons));
    }

    for (int row = 0; row < forced_row_count; ++row) {
        auto& vars_in_row = forced_row_vars[static_cast<std::size_t>(row)];
        if (vars_in_row.empty())
            return {};
        SCIP_CONS* cons = nullptr;
        const std::string name = std::format("forced_{}", row);
        std::vector<SCIP_Real> values(vars_in_row.size(), 1.0);
        check(SCIPcreateConsBasicLinear(
            scip.get(),
            &cons,
            name.c_str(),
            static_cast<int>(vars_in_row.size()),
            vars_in_row.data(),
            values.data(),
            1.0,
            SCIPinfinity(scip.get())
        ));
        check(SCIPaddCons(scip.get(), cons));
        check(SCIPreleaseCons(scip.get(), &cons));
    }

    std::vector<SCIP_VAR*> incumbent_vars;
    incumbent_vars.reserve(vars.size());
    for (SCIP_VAR* var : vars) {
        if (var == nullptr)
            throw std::invalid_argument("SCIP set packing missing variable for column id");
        incumbent_vars.push_back(var);
    }
    static_cast<void>(install_incumbent(scip.get(), incumbent_vars, incumbent_columns));

    if (logging::enabled(log_level)) {
        logging::line(
            "heuristic-colgen: packing SCIP solve start vars=", columns.size(),
            " conflict-rows=", row_count,
            " forced-rows=", forced_row_count,
            " warm-start-vars=", incumbent_columns.size(),
            " objective-offset=", objective_offset,
            " root-cut-rounds=", root_cut_rounds,
            " mode=", root_only ? "root-only" : "full",
            " time-limit=", std::format("{:.1f}s", time_limit_seconds)
        );
    }

    check(SCIPsolve(scip.get()));

    RootSetPackingSolution result;
    SCIP_SOL* const best_sol = SCIPgetBestSol(scip.get());
    if (best_sol == nullptr)
        return result;

    for (int column_id = 0; column_id < column_count; ++column_id) {
        SCIP_VAR* const var = vars[static_cast<std::size_t>(column_id)];
        if (SCIPgetSolVal(scip.get(), best_sol, var) > 0.5) {
            result.columns.push_back(column_id);
            result.saving += savings[static_cast<std::size_t>(column_id)];
        }
    }
    std::ranges::sort(result.columns);
    const int objective = leaf_count + objective_offset - result.saving;

    if (logging::enabled(log_level)) {
        logging::line(
            "heuristic-colgen: packing SCIP solve end status=", SCIPstatusName(SCIPgetStatus(scip.get())),
            " selected-columns=", result.columns.size(),
            " saved=", result.saving,
            " objective=", objective,
            " dual-bound=", SCIPgetDualbound(scip.get()),
            " gap=", SCIPgetGap(scip.get()),
            " nodes=", SCIPgetNNodes(scip.get()),
            " time=", std::format("{:.1f}s", SCIPgetSolvingTime(scip.get()))
        );
    }

    return result;
}

RootSetPackingSolution solve_root_set_packing_highs(
    const int leaf_count,
    const int row_count,
    const int forced_row_count,
    const int column_count,
    const std::span<const RootSetPackingColumnView> columns,
    const std::span<const int> incumbent_columns,
    const int objective_offset,
    const double time_limit_seconds,
    const LogLevel log_level
) {
    if (leaf_count < 0 || row_count < 0 || forced_row_count < 0 || column_count < 0)
        throw std::invalid_argument("HiGHS set packing dimensions must be nonnegative");
    if (time_limit_seconds < 0.0)
        throw std::invalid_argument("HiGHS set packing time limit must be nonnegative");

    Highs highs;
    const bool enable_solver_output = logging::enabled(log_level, LogLevel::VERBOSE);
    check(highs.setOptionValue("output_flag", enable_solver_output));
    check(highs.setOptionValue("log_to_console", false));
    check(highs.setOptionValue("threads", 1));
    if (enable_solver_output) {
        check(highs.setCallback(highs_log_callback));
        check(highs.startCallback(kCallbackLogging));
    }
    if (time_limit_seconds > 0.0)
        check(highs.setOptionValue("time_limit", time_limit_seconds));

    std::vector<int> savings(static_cast<std::size_t>(column_count), 0);
    std::vector<std::vector<HighsInt>> row_columns(static_cast<std::size_t>(row_count));
    std::vector<std::vector<HighsInt>> forced_row_columns(static_cast<std::size_t>(forced_row_count));
    for (const RootSetPackingColumnView& column : columns) {
        validate_column(column, leaf_count, row_count, forced_row_count, column_count);
        if (column.leaves.size() < 2)
            continue;

        const int saving = static_cast<int>(column.leaves.size()) - 1;
        savings[static_cast<std::size_t>(column.column_id)] = saving;
        check(highs.addCol(
            -static_cast<double>(saving),
            0.0,
            1.0,
            0,
            nullptr,
            nullptr
        ));
        check(highs.changeColIntegrality(column.column_id, HighsVarType::kInteger));
        for (const int row : column.row_indices)
            row_columns[static_cast<std::size_t>(row)].push_back(column.column_id);
        for (const int row : column.forced_rows)
            forced_row_columns[static_cast<std::size_t>(row)].push_back(column.column_id);
    }

    for (auto& row : row_columns) {
        if (row.empty())
            continue;
        std::vector<double> values(row.size(), 1.0);
        check(highs.addRow(
            -highs.getInfinity(),
            1.0,
            static_cast<HighsInt>(row.size()),
            row.data(),
            values.data()
        ));
    }
    for (auto& row : forced_row_columns) {
        if (row.empty())
            return {};
        std::vector<double> values(row.size(), 1.0);
        check(highs.addRow(
            1.0,
            highs.getInfinity(),
            static_cast<HighsInt>(row.size()),
            row.data(),
            values.data()
        ));
    }
    check(highs.changeObjectiveOffset(static_cast<double>(leaf_count + objective_offset)));

    if (!incumbent_columns.empty()) {
        HighsSolution incumbent;
        incumbent.value_valid = true;
        incumbent.col_value.assign(static_cast<std::size_t>(column_count), 0.0);
        for (const int column_id : incumbent_columns) {
            if (column_id < 0 || column_id >= column_count)
                throw std::invalid_argument("HiGHS set packing incumbent column id out of range");
            incumbent.col_value[static_cast<std::size_t>(column_id)] = 1.0;
        }
        (void)highs.setSolution(incumbent);
    }

    if (logging::enabled(log_level, LogLevel::VERBOSE)) {
        logging::line(
            "set-packing-highs: start vars=", columns.size(),
            " conflict-rows=", row_count,
            " forced-rows=", forced_row_count,
            " warm-start-vars=", incumbent_columns.size(),
            " objective-offset=", objective_offset,
            " time-limit=", std::format("{:.1f}s", time_limit_seconds)
        );
    }

    check_highs_run(highs.run());

    RootSetPackingSolution result;
    const HighsSolution& solution = highs.getSolution();
    if (!solution.value_valid || solution.col_value.empty())
        return result;

    for (int column_id = 0; column_id < column_count; ++column_id) {
        if (solution.col_value[static_cast<std::size_t>(column_id)] > 0.5) {
            result.columns.push_back(column_id);
            result.saving += savings[static_cast<std::size_t>(column_id)];
        }
    }
    std::ranges::sort(result.columns);

    if (logging::enabled(log_level, LogLevel::VERBOSE)) {
        logging::line(
            "set-packing-highs: end status=", highs.modelStatusToString(highs.getModelStatus()),
            " selected-columns=", result.columns.size(),
            " saved=", result.saving,
            " objective=", leaf_count + objective_offset - result.saving,
            " highs-objective=", highs.getObjectiveValue(),
            " time=", std::format("{:.1f}s", highs.getRunTime())
        );
    }

    return result;
}

} // namespace maffe
