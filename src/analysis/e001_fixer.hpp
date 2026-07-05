#pragma once
#include <string>
#include <string_view>
namespace omni::analysis {
[[nodiscard]] std::string rewrite_e001_to_d(std::string_view text);
}
