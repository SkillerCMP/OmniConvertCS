#include "gui/win32/options_menu.hpp"

#include "gui/win32/app_state.hpp"
#include "gui/win32/armax_options_dialog.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/settings.hpp"

namespace omni::gui::win32 {
namespace {

void update_boolean_checks(HWND window) {
    const AppState* state = app_state(window);
    HMENU menu = GetMenu(window);
    if (state == nullptr || menu == nullptr) return;
    CheckMenuItem(menu, ID_OPTION_MAKEFOLDERS,
                  MF_BYCOMMAND |
                      (state->make_organizers ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, ID_OPTION_CMP_OUTPUT,
                  MF_BYCOMMAND |
                      (state->cmp_output_mode ? MF_CHECKED : MF_UNCHECKED));
    DrawMenuBar(window);
}

} // namespace

void initialize_options_menu(HWND window) {
    update_boolean_checks(window);
}

bool handle_options_menu_command(HWND window, int command_id) {
    AppState* state = app_state(window);
    if (state == nullptr) return false;

    switch (command_id) {
        case ID_OPTION_MAKEFOLDERS:
            state->make_organizers = !state->make_organizers;
            update_boolean_checks(window);
            save_app_settings(*state);
            return true;
        case ID_OPTION_CMP_OUTPUT:
            state->cmp_output_mode = !state->cmp_output_mode;
            update_boolean_checks(window);
            save_app_settings(*state);
            return true;
        case ID_OPTIONS_ARMAX:
            if (show_armax_options_dialog(window)) save_app_settings(*state);
            return true;
        default:
            return false;
    }
}

} // namespace omni::gui::win32
