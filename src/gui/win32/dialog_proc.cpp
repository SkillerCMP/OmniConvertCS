#include "gui/win32/dialog_proc.hpp"

#include "omni/identity.hpp"

#include "gui/win32/app_state.hpp"
#include "gui/win32/command_handlers.hpp"
#include "gui/win32/context_controls.hpp"
#include "gui/win32/file_commands.hpp"
#include "gui/win32/edit_commands.hpp"
#include "gui/win32/format_menu.hpp"
#include "gui/win32/gs3_key_menu.hpp"
#include "gui/win32/layout.hpp"
#include "gui/win32/pnach_crc_dialog.hpp"
#include "gui/win32/options_menu.hpp"
#include "gui/win32/settings.hpp"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"
#include "gui/win32/transpose_mode_menu.hpp"
#include "gui/win32/resources/resource.h"

#include <shellapi.h>

namespace omni::gui::win32 {
namespace {

void initialize_dialog(HWND window, AppState& state) {
    state.window = window;
    set_app_state(window, &state);
    SetWindowTextW(window, omni::identity::window_title_w.data());

    const auto icon = reinterpret_cast<HICON>(LoadImageW(
        state.instance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    }

    HDC device = GetDC(window);
    const int height = -MulDiv(10, GetDeviceCaps(device, LOGPIXELSY), 72);
    ReleaseDC(window, device);
    state.code_font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (state.code_font != nullptr) {
        SendDlgItemMessageW(window, IDC_EDIT_IN, WM_SETFONT,
                            reinterpret_cast<WPARAM>(state.code_font), TRUE);
        SendDlgItemMessageW(window, IDC_EDIT_OUT, WM_SETFONT,
                            reinterpret_cast<WPARAM>(state.code_font), TRUE);
    }

    SendDlgItemMessageW(window, IDC_EDIT_IN, EM_SETLIMITTEXT, 16U * 1024U * 1024U, 0);
    SendDlgItemMessageW(window, IDC_EDIT_OUT, EM_SETLIMITTEXT, 16U * 1024U * 1024U, 0);
    install_input_paste_normalizer(window);
    load_app_settings(state);
    DragAcceptFiles(window, TRUE);
    initialize_format_menus(window);
    initialize_gs3_key_menu(window);
    initialize_options_menu(window);
    initialize_transpose_mode_menu(window);
    initialize_cbc_version_menu(window);
    refresh_pnach_crc_controls(window);
    initialize_context_controls(window);

    RECT client{};
    GetClientRect(window, &client);
    layout_controls(window, client.right, client.bottom);
}

} // namespace

INT_PTR CALLBACK main_dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_INITDIALOG: {
            auto* state = reinterpret_cast<AppState*>(lparam);
            if (state == nullptr) return FALSE;
            initialize_dialog(window, *state);
            return TRUE;
        }
        case WM_COMMAND:
            return handle_command(window, LOWORD(wparam), HIWORD(wparam)) ? TRUE : FALSE;
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) {
                layout_controls(window, LOWORD(lparam), HIWORD(lparam));
            }
            return TRUE;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize.x = 720;
            info->ptMinTrackSize.y = 500;
            return TRUE;
        }
        case WM_DROPFILES:
            load_dropped_file(window, reinterpret_cast<HDROP>(wparam));
            return TRUE;
        case WM_CLOSE:
            if (AppState* state = app_state(window); state != nullptr) {
                save_app_settings(*state);
            }
            DestroyWindow(window);
            return TRUE;
        case WM_DESTROY: {
            if (AppState* state = app_state(window); state != nullptr && state->code_font != nullptr) {
                DeleteObject(state->code_font);
                state->code_font = nullptr;
            }
            PostQuitMessage(0);
            return TRUE;
        }
        default:
            return FALSE;
    }
}

} // namespace omni::gui::win32
