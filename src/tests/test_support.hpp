#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace omni::tests {

inline void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

} // namespace omni::tests
