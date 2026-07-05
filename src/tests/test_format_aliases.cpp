#include "tests/test_format_aliases.hpp"

#include "convert/conversion_service.hpp"
#include "core/code_format.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/test_support.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace omni::tests {
namespace {

std::vector<std::uint32_t> first_block_words(const convert::Result& result, Crypt crypt) {
    const auto blocks = formats::text::parse_text(result.output_text, crypt);
    for (const auto& block : blocks) {
        if (!block.cheat.empty()) return block.cheat.words;
    }
    return {};
}

convert::Result convert_sample(CodeFormat output, std::uint8_t gs_key = 2U) {
    constexpr std::string_view sample =
        "Alias Test\n"
        "20123456 DEADBEEF\n";

    const convert::Request request{
        CodeFormat::standard_raw,
        output,
        devices::action_replay::ar1_seed,
        gs_key,
        devices::armax::default_payload_key};
    return convert::convert_text(sample, request);
}

void test_alias_parsing() {
    require(parse_code_format("GS1") == CodeFormat::gameshark_interact1,
            "GS1 format alias parse failed");
    require(parse_code_format("IA2") == CodeFormat::gameshark_interact2,
            "IA2 format alias parse failed");
    require(parse_code_format("Swap-Magic") == CodeFormat::swap_magic_coder3,
            "Swap Magic format alias parse failed");
    require(parse_code_format("XP1-3") == CodeFormat::xploder1_3,
            "Xploder V1-V3 format alias parse failed");
    require(parse_code_format("XP4") == CodeFormat::xploder4,
            "Xploder V4 format alias parse failed");
    require(parse_code_format("XP5") == CodeFormat::xploder5,
            "Xploder V5 format alias parse failed");
    require(parse_code_format("GS12RAW") == CodeFormat::gameshark12_raw,
            "GameShark V1-V2 RAW format parse failed");
    require(parse_code_format("PS2-MIPS-R5900") == CodeFormat::ps2_mips_r5900,
            "PS2 MIPS R5900 format alias parse failed");
}

void test_alias_crypto_mapping() {
    require(crypt_for_format(CodeFormat::gameshark_interact1) == Crypt::ar1,
            "Interact GameShark V1 should use AR1 crypto");
    require(crypt_for_format(CodeFormat::swap_magic_coder3) == Crypt::ar1,
            "Swap Magic Coder should use AR1 crypto");
    require(crypt_for_format(CodeFormat::gameshark_interact2) == Crypt::ar2,
            "Interact GameShark V2 should use AR2 crypto");
    require(crypt_for_format(CodeFormat::xploder1_3) == Crypt::codebreaker,
            "Xploder V1-V3 should use CodeBreaker crypto");
    require(crypt_for_format(CodeFormat::xploder4) == Crypt::gameshark3,
            "Xploder V4 should use GameShark V3/V4 crypto");
    require(crypt_for_format(CodeFormat::xploder5) == Crypt::gameshark5,
            "Xploder V5 should use GameShark V5 crypto");

    require(device_for_format(CodeFormat::gameshark_interact1) == Device::ar1,
            "Interact GameShark V1 device mapping failed");
    require(device_for_format(CodeFormat::gameshark_interact2) == Device::ar2,
            "Interact GameShark V2 device mapping failed");
    require(device_for_format(CodeFormat::xploder1_3) == Device::codebreaker,
            "Xploder V1-V3 device mapping failed");
    require(device_for_format(CodeFormat::xploder4) == Device::gameshark3,
            "Xploder V4 device mapping failed");
}

void test_alias_outputs_match_base_crypto() {
    const auto ar1 = convert_sample(CodeFormat::action_replay1);
    const auto gs1 = convert_sample(CodeFormat::gameshark_interact1);
    const auto smc = convert_sample(CodeFormat::swap_magic_coder3);
    require(first_block_words(ar1, Crypt::ar1) == first_block_words(gs1, Crypt::ar1),
            "GameShark V1 output differs from AR1 crypto");
    require(first_block_words(ar1, Crypt::ar1) == first_block_words(smc, Crypt::ar1),
            "Swap Magic output differs from AR1 crypto");

    const auto ar2 = convert_sample(CodeFormat::action_replay2);
    const auto gs2 = convert_sample(CodeFormat::gameshark_interact2);
    require(first_block_words(ar2, Crypt::ar2) == first_block_words(gs2, Crypt::ar2),
            "GameShark V2 output differs from AR2 crypto");

    const auto cb = convert_sample(CodeFormat::codebreaker1_6);
    const auto xp13 = convert_sample(CodeFormat::xploder1_3);
    require(first_block_words(cb, Crypt::codebreaker) ==
                first_block_words(xp13, Crypt::codebreaker),
            "Xploder V1-V3 output differs from CodeBreaker crypto");

    const auto gs3 = convert_sample(CodeFormat::gameshark_madcatz34, 2U);
    const auto xp4 = convert_sample(CodeFormat::xploder4, 2U);
    require(first_block_words(gs3, Crypt::gameshark3) ==
                first_block_words(xp4, Crypt::gameshark3),
            "Xploder V4 output differs from GameShark V3/V4 crypto");

    const auto gs5 = convert_sample(CodeFormat::gameshark_madcatz5, 2U);
    const auto xp5 = convert_sample(CodeFormat::xploder5, 2U);
    require(first_block_words(gs5, Crypt::gameshark5) ==
                first_block_words(xp5, Crypt::gameshark5),
            "Xploder V5 output differs from GameShark V5 crypto");
}

void test_every_menu_format_is_enabled() {
    constexpr std::array<CodeFormat, 21> formats{{
        CodeFormat::standard_raw,
        CodeFormat::pnach_raw,
        CodeFormat::ps2_mips_r5900,
        CodeFormat::armax_raw,
        CodeFormat::ar12_raw,
        CodeFormat::codebreaker_raw,
        CodeFormat::gameshark12_raw,
        CodeFormat::gameshark3_raw,
        CodeFormat::action_replay1,
        CodeFormat::action_replay2,
        CodeFormat::action_replay_max,
        CodeFormat::codebreaker1_6,
        CodeFormat::codebreaker7_common,
        CodeFormat::gameshark_interact1,
        CodeFormat::gameshark_interact2,
        CodeFormat::gameshark_madcatz34,
        CodeFormat::gameshark_madcatz5,
        CodeFormat::swap_magic_coder3,
        CodeFormat::xploder1_3,
        CodeFormat::xploder4,
        CodeFormat::xploder5,
    }};

    for (const CodeFormat format : formats) {
        require(convert::is_implemented(format),
                "A user-facing crypto format is still disabled");
    }
}

} // namespace

void run_format_alias_tests() {
    test_alias_parsing();
    test_alias_crypto_mapping();
    test_alias_outputs_match_base_crypto();
    test_every_menu_format_is_enabled();
}

} // namespace omni::tests
