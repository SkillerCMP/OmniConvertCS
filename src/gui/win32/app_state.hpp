#pragma once

#include "core/code_format.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "devices/armax/armax_crypto.hpp"
#include "devices/gameshark/gs3_crypto.hpp"
#include "translate/transpose_mode.hpp"
#include "convert/conversion_service.hpp"
#include "formats/binary/application_formats.hpp"
#include "formats/text/text_cheat_parser.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

namespace omni::gui::win32 {

enum class ArmaxDiscHashSource : std::uint8_t {
    none = 0U,
    drive = 1U,
    manual = 2U,
};

struct AppState {
    HINSTANCE instance{};
    HWND window{};
    CodeFormat input_format{CodeFormat::standard_raw};
    CodeFormat output_format{CodeFormat::standard_raw};
    std::uint32_t ar2_input_key{devices::action_replay::ar1_seed};
    std::uint32_t ar2_output_key{devices::action_replay::ar1_seed};
    std::uint8_t gs3_key{devices::gameshark::default_key};
    std::uint32_t armax_key{devices::armax::default_payload_key};
    std::uint32_t armax_game_id{0x0357U};
    std::uint8_t armax_region{};
    convert::ArmaxVerifierMode armax_verifier_mode{convert::ArmaxVerifierMode::automatic};
    bool make_organizers{};
    ArmaxDiscHashSource armax_disc_hash_source{ArmaxDiscHashSource::none};
    char armax_hash_drive{};
    std::optional<std::uint32_t> armax_manual_disc_hash;
    std::string armax_disc_hash_game_name;
    std::string armax_disc_hash_elf_name;
    translate::TransposeMode transpose_mode{translate::TransposeMode::original};
    bool cmp_output_mode{};
    bool mgs_c_type_pointer_mode{};
    bool pnach_crc_active{};
    std::optional<std::uint32_t> pnach_crc;
    std::string pnach_game_name;
    std::string pnach_elf_name;
    formats::binary::CbcVersion cbc_version{formats::binary::CbcVersion::v7};
    std::vector<formats::text::CheatBlock> last_converted_blocks;
    std::optional<CodeFormat> last_output_format;
    std::uint32_t last_game_id{};
    bool syncing_game_id{};
    HFONT code_font{};
};

[[nodiscard]] AppState* app_state(HWND window) noexcept;
void set_app_state(HWND window, AppState* state) noexcept;

} // namespace omni::gui::win32
