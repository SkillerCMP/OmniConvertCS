#include "gui/win32/transpose_mode_menu.hpp"

#include "gui/win32/app_state.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/settings.hpp"
#include "translate/transpose_mode.hpp"

namespace omni::gui::win32 {
namespace {

void update_checks(HWND window, translate::TransposeMode mode) {
    const UINT selected = mode == translate::TransposeMode::original
        ? ID_TRANSPOSE_ORIGINAL
        : ID_TRANSPOSE_STRICT;
    CheckMenuRadioItem(GetMenu(window), ID_TRANSPOSE_STRICT,
                       ID_TRANSPOSE_ORIGINAL, selected, MF_BYCOMMAND);
}

} // namespace

void initialize_transpose_mode_menu(HWND window) {
    AppState* state = app_state(window);
    if (state == nullptr) return;
    update_checks(window, state->transpose_mode);
}

bool handle_transpose_mode_command(HWND window, int command_id) {
    AppState* state = app_state(window);
    if (state == nullptr) return false;

    switch (command_id) {
        case ID_TRANSPOSE_STRICT:
            state->transpose_mode = translate::TransposeMode::strict;
            update_checks(window, state->transpose_mode);
            save_app_settings(*state);
            return true;
        case ID_TRANSPOSE_ORIGINAL:
            state->transpose_mode = translate::TransposeMode::original;
            update_checks(window, state->transpose_mode);
            save_app_settings(*state);
            return true;
        default:
            return false;
    }
}

} // namespace omni::gui::win32
