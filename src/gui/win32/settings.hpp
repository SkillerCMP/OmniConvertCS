#pragma once

#include "gui/win32/app_state.hpp"

namespace omni::gui::win32 {

void load_app_settings(AppState& state) noexcept;
void save_app_settings(const AppState& state) noexcept;

} // namespace omni::gui::win32
