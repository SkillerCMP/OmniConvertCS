#pragma once

#include "core/crypt.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace omni {

[[nodiscard]] std::optional<Crypt> crypt_from_label(std::string_view label);
[[nodiscard]] std::string strip_crypt_tag(std::string_view label);
[[nodiscard]] std::string update_existing_crypt_tag(std::string_view label, Crypt output_crypt);

} // namespace omni
