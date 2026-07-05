#pragma once

#include "core/cheat.hpp"
#include "core/crypt.hpp"
#include "translate/transpose_mode.hpp"

#include <string>
#include <vector>

namespace omni::translate {

struct Report {
    std::vector<std::string> warnings;
};

void translate_cheat(Cheat& cheat, Device input_device, Device output_device,
                     Report* report = nullptr,
                     TransposeMode mode = TransposeMode::strict);

} // namespace omni::translate
