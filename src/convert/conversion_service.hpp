#pragma once

#include "core/code_format.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "devices/armax/armax_crypto.hpp"
#include "devices/gameshark/gs3_crypto.hpp"
#include "translate/transpose_mode.hpp"
#include "formats/text/text_cheat_parser.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <string_view>
#include <vector>

namespace omni::convert {

enum class ArmaxVerifierMode {
    automatic,
    manual
};

struct Request {
    CodeFormat input_format{CodeFormat::standard_raw};
    CodeFormat output_format{CodeFormat::standard_raw};
    std::uint32_t ar2_key{devices::action_replay::ar1_seed};
    std::uint32_t input_ar2_key{devices::action_replay::ar1_seed};
    std::uint32_t output_ar2_key{devices::action_replay::ar1_seed};
    std::uint8_t gs3_key{devices::gameshark::default_key};
    std::uint32_t armax_key{devices::armax::default_payload_key};
    std::uint32_t armax_game_id{};
    std::uint8_t armax_region{};
    std::optional<std::uint32_t> armax_verifier_nonce;
    ArmaxVerifierMode armax_verifier_mode{ArmaxVerifierMode::manual};
    bool make_organizers{};
    std::uint32_t armax_disc_hash{};
    translate::TransposeMode transpose_mode{translate::TransposeMode::original};
    bool cmp_output_mode{};
    bool mgs_c_type_pointer_mode{};
    std::string game_name;
    bool pnach_include_crc{};
    std::optional<std::uint32_t> pnach_crc;
    bool continue_on_error{};

    Request() = default;

    Request(CodeFormat input, CodeFormat output,
            std::uint32_t ar2 = devices::action_replay::ar1_seed,
            std::uint8_t gs3 = devices::gameshark::default_key,
            std::uint32_t armax = devices::armax::default_payload_key,
            std::uint32_t game_id = 0U, std::uint8_t region = 0U,
            std::optional<std::uint32_t> verifier_nonce = std::nullopt,
            translate::TransposeMode mode = translate::TransposeMode::original,
            std::string pnach_game_name = {}, bool include_pnach_crc = false,
            std::optional<std::uint32_t> pnach_crc_value = std::nullopt)
        : input_format(input), output_format(output), ar2_key(ar2),
          input_ar2_key(ar2), output_ar2_key(ar2), gs3_key(gs3),
          armax_key(armax), armax_game_id(game_id), armax_region(region),
          armax_verifier_nonce(verifier_nonce), transpose_mode(mode),
          game_name(std::move(pnach_game_name)),
          pnach_include_crc(include_pnach_crc), pnach_crc(pnach_crc_value) {}
};

struct ConversionIssue {
    std::string cheat_name;
    std::string stage;
    std::string message;
};

struct Result {
    std::string output_text;
    std::vector<std::string> warnings;
    std::vector<formats::text::CheatBlock> blocks;
    std::optional<std::uint32_t> detected_armax_game_id;
    std::optional<std::uint8_t> detected_armax_region;
    std::optional<std::uint32_t> active_ar2_seed;
    std::vector<ConversionIssue> issues;
};

[[nodiscard]] bool is_implemented(Crypt crypt) noexcept;
[[nodiscard]] bool is_implemented(CodeFormat format) noexcept;
[[nodiscard]] Result convert_text(std::string_view input, const Request& request);

} // namespace omni::convert
