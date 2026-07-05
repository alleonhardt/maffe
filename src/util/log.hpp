#pragma once

#include "maffe.hpp"

#include <iostream>
#include <utility>

namespace maffe::logging {

[[nodiscard]] constexpr bool enabled(
    const LogLevel level,
    const LogLevel threshold = LogLevel::NORMAL
) {
    return static_cast<int>(level) >= static_cast<int>(threshold);
}

template<class... Ts>
void line(Ts&&... xs) {
    std::cerr << "# ";
    (std::cerr << ... << std::forward<Ts>(xs));
    std::cerr << '\n';
}

} // namespace maffe::logging
