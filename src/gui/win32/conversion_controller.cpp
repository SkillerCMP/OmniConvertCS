#include "gui/win32/conversion_controller.hpp"

#include "convert/conversion_service.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/format_menu.hpp"
#include "gui/win32/context_controls.hpp"
#include "gui/win32/armax_options_dialog.hpp"
#include "gui/win32/settings.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/text_control.hpp"
#include "util/hex.hpp"

#include <exception>
#include <stdexcept>
#include <string>

namespace omni::gui::win32 {

void set_status(HWND window, const wchar_t* message) noexcept {
    SetDlgItemTextW(window, ID_STATIC_STATUS, message != nullptr ? message : L"");
}

void run_conversion(HWND window) {
    AppState* state = app_state(window);
    if (state == nullptr) return;

    try {
        set_status(window, L"Converting...");
        const std::string input = read_window_text_utf8(GetDlgItem(window, IDC_EDIT_IN));
        convert::Request request;
        request.input_format = state->input_format;
        request.output_format = state->output_format;
        request.input_ar2_key = state->ar2_input_key;
        request.output_ar2_key = state->ar2_output_key;
        request.gs3_key = state->gs3_key;
        request.armax_key = state->armax_key;
        request.armax_region = state->armax_region;
        request.armax_verifier_mode = state->armax_verifier_mode;
        request.make_organizers = state->make_organizers;
        if (state->output_format == CodeFormat::action_replay_max ||
            state->output_format == CodeFormat::armax_raw) {
            switch (state->armax_disc_hash_source) {
                case ArmaxDiscHashSource::manual:
                    request.armax_disc_hash =
                        state->armax_manual_disc_hash.value_or(0U);
                    break;
                case ArmaxDiscHashSource::drive:
                    request.armax_disc_hash =
                        compute_armax_disc_hash(window, state->armax_hash_drive);
                    break;
                case ArmaxDiscHashSource::none:
                default:
                    request.armax_disc_hash = 0U;
                    break;
            }
        }
        request.transpose_mode = state->transpose_mode;
        request.cmp_output_mode = state->cmp_output_mode;
        request.mgs_c_type_pointer_mode = state->mgs_c_type_pointer_mode;
        request.continue_on_error = true;
        request.armax_game_id = state->armax_game_id;
        const std::string ui_game_name =
            read_window_text_utf8(GetDlgItem(window, IDC_EDIT_GAMENAME));
        request.game_name = state->pnach_game_name.empty()
                                ? ui_game_name
                                : state->pnach_game_name;
        request.pnach_include_crc = state->pnach_crc_active;
        request.pnach_crc = state->pnach_crc;

        const convert::Result result = convert::convert_text(input, request);
        state->last_converted_blocks = result.blocks;
        state->last_output_format = state->output_format;
        state->last_game_id = request.armax_game_id;
        if (result.detected_armax_game_id) {
            state->armax_game_id = *result.detected_armax_game_id;
            sync_game_id_controls(window);
        }
        if (result.detected_armax_region) {
            state->armax_region = *result.detected_armax_region;
        }
        if (result.active_ar2_seed) {
            state->ar2_input_key = *result.active_ar2_seed;
            refresh_ar2_key_display(window);
        }
        save_app_settings(*state);
        set_window_text_utf8(GetDlgItem(window, IDC_EDIT_OUT), result.output_text);

        if (!result.issues.empty()) {
            const std::wstring message = L"Completed with " +
                std::to_wstring(result.issues.size()) + L" error block(s)";
            SetDlgItemTextW(window, ID_STATIC_STATUS, message.c_str());
        } else if (result.warnings.empty()) {
            set_status(window, L"Conversion completed");
        } else {
            std::wstring message = L"Completed with warning: ";
            message += utf8_to_utf16(result.warnings.front());
            SetDlgItemTextW(window, ID_STATIC_STATUS, message.c_str());
        }
    } catch (const std::exception& error) {
        const std::wstring message = utf8_to_utf16(error.what());
        set_status(window, L"Conversion failed");
        MessageBoxW(window, message.c_str(), L"OmniConvert", MB_OK | MB_ICONERROR);
    }
}

void swap_input_output(HWND window) {
    AppState* state = app_state(window);
    if (state == nullptr) return;

    HWND input = GetDlgItem(window, IDC_EDIT_IN);
    HWND output = GetDlgItem(window, IDC_EDIT_OUT);
    const std::string input_text = read_window_text_utf8(input);
    const std::string output_text = read_window_text_utf8(output);
    set_window_text_utf8(input, output_text);
    set_window_text_utf8(output, input_text);

    state->last_converted_blocks.clear();
    state->last_output_format.reset();

    const CodeFormat old_input = state->input_format;
    select_input_format(window, state->output_format);
    select_output_format(window, old_input == CodeFormat::inline_headers
                                     ? CodeFormat::standard_raw
                                     : old_input);
    set_status(window, L"Input and output swapped");
}

} // namespace omni::gui::win32
