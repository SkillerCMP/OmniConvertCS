#include "gui/win32/format_menu.hpp"

#include "convert/conversion_service.hpp"
#include "gui/win32/app_state.hpp"
#include "gui/win32/resources/resource.h"
#include "gui/win32/pnach_crc_dialog.hpp"
#include "gui/win32/context_controls.hpp"

#include <array>
#include <string>

namespace omni::gui::win32 {
namespace {

struct Binding {
    int command;
    bool input;
    CodeFormat format;
};

constexpr std::array<Binding, 43> bindings{{
    {ID_INPUT_UNENC_COMMON, true, CodeFormat::standard_raw},
    {ID_INPUT_UNENC_INLINE, true, CodeFormat::inline_headers},
    {ID_INPUT_UNENC_MIPS, true, CodeFormat::ps2_mips_r5900},
    {ID_INPUT_UNENC_PNACH, true, CodeFormat::pnach_raw},
    {ID_INPUT_UNENC_ARMAX, true, CodeFormat::armax_raw},
    {ID_INPUT_UNENC_AR1_2, true, CodeFormat::ar12_raw},
    {ID_INPUT_UNENC_CB, true, CodeFormat::codebreaker_raw},
    {ID_INPUT_UNENC_GS1_2, true, CodeFormat::gameshark12_raw},
    {ID_INPUT_UNENC_XP, true, CodeFormat::gameshark3_raw},
    {ID_INPUT_AR2_V1, true, CodeFormat::action_replay1},
    {ID_INPUT_AR2_V2, true, CodeFormat::action_replay2},
    {ID_INPUT_ARMAX, true, CodeFormat::action_replay_max},
    {ID_INPUT_CB, true, CodeFormat::codebreaker1_6},
    {ID_INPUT_CB7_COMMON, true, CodeFormat::codebreaker7_common},
    {ID_INPUT_IA_V1, true, CodeFormat::gameshark_interact1},
    {ID_INPUT_IA_V2, true, CodeFormat::gameshark_interact2},
    {ID_INPUT_MC_V3, true, CodeFormat::gameshark_madcatz34},
    {ID_INPUT_MC_V5, true, CodeFormat::gameshark_madcatz5},
    {ID_INPUT_SMC, true, CodeFormat::swap_magic_coder3},
    {ID_INPUT_XP_V1, true, CodeFormat::xploder1_3},
    {ID_INPUT_XP_V4, true, CodeFormat::xploder4},
    {ID_INPUT_XP_V5, true, CodeFormat::xploder5},

    {ID_OUTPUT_UNENC_COMMON, false, CodeFormat::standard_raw},
    {ID_OUTPUT_UNENC_MIPS, false, CodeFormat::ps2_mips_r5900},
    {ID_OUTPUT_UNENC_PNACH, false, CodeFormat::pnach_raw},
    {ID_OUTPUT_UNENC_ARMAX, false, CodeFormat::armax_raw},
    {ID_OUTPUT_UNENC_AR1_2, false, CodeFormat::ar12_raw},
    {ID_OUTPUT_UNENC_CB, false, CodeFormat::codebreaker_raw},
    {ID_OUTPUT_UNENC_GS1_2, false, CodeFormat::gameshark12_raw},
    {ID_OUTPUT_UNENC_XP, false, CodeFormat::gameshark3_raw},
    {ID_OUTPUT_AR2_V1, false, CodeFormat::action_replay1},
    {ID_OUTPUT_AR2_V2, false, CodeFormat::action_replay2},
    {ID_OUTPUT_ARMAX, false, CodeFormat::action_replay_max},
    {ID_OUTPUT_CB, false, CodeFormat::codebreaker1_6},
    {ID_OUTPUT_CB7_COMMON, false, CodeFormat::codebreaker7_common},
    {ID_OUTPUT_IA_V1, false, CodeFormat::gameshark_interact1},
    {ID_OUTPUT_IA_V2, false, CodeFormat::gameshark_interact2},
    {ID_OUTPUT_MC_V3, false, CodeFormat::gameshark_madcatz34},
    {ID_OUTPUT_MC_V5, false, CodeFormat::gameshark_madcatz5},
    {ID_OUTPUT_SMC, false, CodeFormat::swap_magic_coder3},
    {ID_OUTPUT_XP_V1, false, CodeFormat::xploder1_3},
    {ID_OUTPUT_XP_V4, false, CodeFormat::xploder4},
    {ID_OUTPUT_XP_V5, false, CodeFormat::xploder5},
}};

void update_checks(HWND window, bool input, CodeFormat selected) {
    HMENU menu = GetMenu(window);
    if (menu == nullptr) return;

    for (const Binding& binding : bindings) {
        if (binding.input != input) continue;
        CheckMenuItem(menu, static_cast<UINT>(binding.command),
                      MF_BYCOMMAND | (binding.format == selected ? MF_CHECKED : MF_UNCHECKED));
    }
}

void update_label(HWND window, bool input, CodeFormat format) {
    std::wstring text = input ? L"Input: " : L"Output: ";
    text += format_label(format);
    SetDlgItemTextW(window, input ? ID_STATIC_INPUT : ID_STATIC_OUTPUT, text.c_str());
}

} // namespace

const wchar_t* format_label(CodeFormat format) noexcept {
    switch (format) {
        case CodeFormat::standard_raw: return L"RAW / Standard";
        case CodeFormat::inline_headers: return L"INLINE (CRYPT_ headers)";
        case CodeFormat::pnach_raw: return L"PNACH (RAW)";
        case CodeFormat::ps2_mips_r5900: return L"PS2 MIPS (R5900)";
        case CodeFormat::armax_raw: return L"ARMAX RAW";
        case CodeFormat::ar12_raw: return L"Action Replay / GameShark V1-V2 RAW";
        case CodeFormat::codebreaker_raw: return L"CodeBreaker RAW";
        case CodeFormat::gameshark12_raw: return L"GameShark V1-V2 RAW";
        case CodeFormat::gameshark3_raw: return L"Xploder / GameShark V3 RAW";
        case CodeFormat::action_replay1: return L"Action Replay V1";
        case CodeFormat::action_replay2: return L"Action Replay V2";
        case CodeFormat::action_replay_max: return L"Action Replay MAX";
        case CodeFormat::codebreaker1_6: return L"CodeBreaker V1-V6";
        case CodeFormat::codebreaker7_common: return L"CodeBreaker V7+ Common";
        case CodeFormat::gameshark_interact1: return L"Interact GameShark V1";
        case CodeFormat::gameshark_interact2: return L"Interact GameShark V2";
        case CodeFormat::gameshark_madcatz34: return L"MadCatz GameShark V3-V4";
        case CodeFormat::gameshark_madcatz5: return L"MadCatz GameShark V5+";
        case CodeFormat::swap_magic_coder3: return L"Swap Magic Coder V3.x";
        case CodeFormat::xploder1_3: return L"Xploder V1-V3";
        case CodeFormat::xploder4: return L"Xploder V4";
        case CodeFormat::xploder5: return L"Xploder V5+";
    }
    return L"Unknown";
}

void initialize_format_menus(HWND window) {
    HMENU menu = GetMenu(window);
    if (menu == nullptr) return;

    for (const Binding& binding : bindings) {
        const bool implemented = convert::is_implemented(binding.format);
        EnableMenuItem(menu, static_cast<UINT>(binding.command),
                       MF_BYCOMMAND | (implemented ? MF_ENABLED : MF_GRAYED));
    }

    if (AppState* state = app_state(window); state != nullptr) {
        select_input_format(window, state->input_format);
        select_output_format(window, state->output_format);
    }
    DrawMenuBar(window);
}

std::optional<MenuFormatSelection> menu_format_selection(int command_id) noexcept {
    for (const Binding& binding : bindings) {
        if (binding.command == command_id) {
            return MenuFormatSelection{binding.input, binding.format};
        }
    }
    return std::nullopt;
}

void select_input_format(HWND window, CodeFormat format) {
    if (AppState* state = app_state(window); state != nullptr) state->input_format = format;
    update_checks(window, true, format);
    update_label(window, true, format);
    refresh_context_controls(window);
}

void select_output_format(HWND window, CodeFormat format) {
    if (AppState* state = app_state(window); state != nullptr) {
        state->output_format = format;
        if (format != CodeFormat::pnach_raw) state->pnach_crc_active = false;
    }
    update_checks(window, false, format);
    update_label(window, false, format);
    refresh_pnach_crc_controls(window);
    refresh_context_controls(window);
}

} // namespace omni::gui::win32
