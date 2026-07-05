#pragma once

#include <string>
#include <string_view>

namespace omni::formats::text {

// Normalizes text copied from legacy CodeBreaker pages, GameHacking.org, and
// CMP-style databases into OmniConvert's ordinary label/code layout. The
// transformation is intentionally conservative and idempotent so it is safe
// to run when pasting text or loading a text file.
[[nodiscard]] std::string clean_copy_paste_text(std::string_view input);

} // namespace omni::formats::text
