#pragma once

#include <optional>
#include <string>
#include <vector>

namespace maffe {

enum class LogLevel : unsigned char {
    QUIET,
    NORMAL,
    VERBOSE,
};

struct SolveOptions {
    std::optional<double> timeout_seconds;
    double acceptable_factor = 1.0;
    int acceptable_offset = 0;
    LogLevel log_level = LogLevel::NORMAL;
    bool heuristic_mode = false;
};

std::vector<std::vector<std::string>> solve(const std::vector<std::string>& trees);
std::vector<std::vector<std::string>> solve(const std::vector<std::string>& trees, const SolveOptions& options);

std::string restrict(const std::vector<std::string>& trees, const std::vector<std::string>& leaves);
std::vector<std::string> restrict(
    const std::vector<std::string>& trees,
    const std::vector<std::vector<std::string>>& partition
);

} // namespace maffe
