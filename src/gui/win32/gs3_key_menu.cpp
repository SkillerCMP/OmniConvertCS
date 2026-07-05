#include "gui/win32/gs3_key_menu.hpp"

#include "gui/win32/app_state.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/settings.hpp"

#include <array>
#include <cstdint>

namespace omni::gui::win32 {
namespace {

struct KeyBinding {
    int command_id;
    std::uint8_t key;
};

constexpr std::array<KeyBinding, 5> key_bindings{{
    {ID_GS3_KEY_0, 0U},
    {ID_GS3_KEY_1, 1U},
    {ID_GS3_KEY_2, 2U},
    {ID_GS3_KEY_3, 3U},
    {ID_GS3_KEY_4, 4U},
}};

void update_checks(HWND window, std::uint8_t selected_key) {
    HMENU menu = GetMenu(window);
    if (menu == nullptr) return;

    for (const KeyBinding& binding : key_bindings) {
        CheckMenuItem(menu, static_cast<UINT>(binding.command_id),
                      MF_BYCOMMAND |
                          (binding.key == selected_key ? MF_CHECKED : MF_UNCHECKED));
    }
    DrawMenuBar(window);
}

} // namespace

void initialize_gs3_key_menu(HWND window) {
    if (const AppState* state = app_state(window); state != nullptr) {
        update_checks(window, state->gs3_key);
    }
}

bool handle_gs3_key_command(HWND window, int command_id) {
    for (const KeyBinding& binding : key_bindings) {
        if (binding.command_id != command_id) continue;

        if (AppState* state = app_state(window); state != nullptr) {
            state->gs3_key = binding.key;
            update_checks(window, binding.key);
            save_app_settings(*state);
        }
        return true;
    }
    return false;
}

} // namespace omni::gui::win32
