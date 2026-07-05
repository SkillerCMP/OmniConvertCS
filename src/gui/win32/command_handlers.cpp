#include "gui/win32/command_handlers.hpp"

#include "gui/win32/conversion_controller.hpp"
#include "gui/win32/context_controls.hpp"
#include "gui/win32/edit_commands.hpp"
#include "gui/win32/file_commands.hpp"
#include "gui/win32/format_menu.hpp"
#include "gui/win32/gs3_key_menu.hpp"
#include "gui/win32/key_dialog.hpp"
#include "gui/win32/pnach_crc_dialog.hpp"
#include "gui/win32/options_menu.hpp"
#include "gui/win32/transpose_mode_menu.hpp"
#include "gui/win32/resources/resource.h"

namespace omni::gui::win32 {
bool handle_command(HWND window, int command_id, int notification_code) {
    if (const auto selection = menu_format_selection(command_id); selection.has_value()) {
        if (selection->is_input) select_input_format(window, selection->format);
        else select_output_format(window, selection->format);
        return true;
    }

    if (handle_context_control_command(window, command_id, notification_code)) return true;
    if (handle_edit_command(window, command_id)) return true;
    if (handle_gs3_key_command(window, command_id)) return true;
    if (handle_transpose_mode_command(window, command_id)) return true;
    if (handle_options_menu_command(window, command_id)) return true;
    if (handle_pnach_crc_command(window, command_id, notification_code)) return true;
    if (handle_cbc_version_command(window, command_id)) return true;

    switch (command_id) {
        case ID_BTN_CONVERT:
            if (notification_code == BN_CLICKED) run_conversion(window);
            return true;
        case ID_BTN_CLEAR_IN:
            if (notification_code == BN_CLICKED) SetDlgItemTextW(window, IDC_EDIT_IN, L"");
            return true;
        case ID_EDIT_CLEAR_INPUT:
            SetDlgItemTextW(window, IDC_EDIT_IN, L"");
            return true;
        case ID_BTN_CLEAR_OUT:
            if (notification_code == BN_CLICKED) SetDlgItemTextW(window, IDC_EDIT_OUT, L"");
            return true;
        case ID_EDIT_CLEAR_OUTPUT:
            SetDlgItemTextW(window, IDC_EDIT_OUT, L"");
            return true;
        case ID_BTN_SWAP:
        case ID_EDIT_SWAP:
            swap_input_output(window);
            return true;
        case ID_FILE_LOADTEXT:
            open_text_file(window);
            return true;
        case ID_FILE_LOADCBC:
            open_application_file(window, formats::binary::ApplicationFormat::codebreaker_cbc);
            return true;
        case ID_FILE_LOADARMAXBIN:
            open_application_file(window, formats::binary::ApplicationFormat::armax_bin);
            return true;
        case ID_FILE_LOADP2M:
            open_application_file(window, formats::binary::ApplicationFormat::gameshark_p2m);
            return true;
        case ID_FILE_LOADSMC:
            open_application_file(window, formats::binary::ApplicationFormat::swap_magic_bin);
            return true;
        case ID_FILE_SAVEASTEXT:
            save_output_text(window);
            return true;
        case ID_FILE_SAVEASARMAX:
            save_application_file(window, formats::binary::ApplicationFormat::armax_bin);
            return true;
        case ID_FILE_SAVEASCBC:
            save_application_file(window, formats::binary::ApplicationFormat::codebreaker_cbc);
            return true;
        case ID_FILE_SAVEASP2M:
            save_application_file(window, formats::binary::ApplicationFormat::gameshark_p2m);
            return true;
        case ID_FILE_SAVEASSMC:
            save_application_file(window, formats::binary::ApplicationFormat::swap_magic_bin);
            return true;
        case ID_AR2_KEY:
            show_ar2_key_dialog(window);
            return true;
        case ID_FILE_EXIT:
        case IDCANCEL:
            SendMessageW(window, WM_CLOSE, 0, 0);
            return true;
        default:
            return false;
    }
}

} // namespace omni::gui::win32
