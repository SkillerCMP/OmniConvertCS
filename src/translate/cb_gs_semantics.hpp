#pragma once

#include "core/cheat.hpp"
#include "core/crypt.hpp"
#include "translate/translator.hpp"

#include <cstddef>

namespace omni::translate::detail {

[[nodiscard]] bool is_codebreaker_gameshark_pair(Device input,
                                                  Device output) noexcept;

void translate_codebreaker_gameshark(Cheat& destination,
                                     const Cheat& source,
                                     std::size_t& index,
                                     Device input,
                                     Device output,
                                     Report* report);

} // namespace omni::translate::detail
