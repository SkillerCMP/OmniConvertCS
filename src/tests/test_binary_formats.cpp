#include "tests/test_binary_formats.hpp"

#include "convert/conversion_service.hpp"
#include "devices/gameshark/gs3_file.hpp"
#include "formats/binary/application_formats.hpp"
#include "formats/binary/binary_common.hpp"
#include "tests/test_support.hpp"
#include "translate/transpose_mode.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <stdexcept>
#include <string>
#include <vector>

namespace omni::tests {
namespace {

formats::text::CheatBlock make_block(std::string name,
                                     std::initializer_list<std::uint32_t> words,
                                     bool master = false) {
    formats::text::CheatBlock block;
    block.label = name;
    block.cheat.name = std::move(name);
    block.cheat.words.assign(words.begin(), words.end());
    block.cheat.flags[Cheat::flag_master_code] = master ? 1U : 0U;
    return block;
}

std::vector<formats::text::CheatBlock> sample_blocks() {
    std::vector<formats::text::CheatBlock> blocks;
    blocks.push_back(make_block("(M)", {0xF0145714U, 0x00145717U}, true));

    formats::text::CheatBlock heading;
    heading.label = "Player Codes";
    heading.cheat.name = "Player Codes";
    blocks.push_back(std::move(heading));

    blocks.push_back(make_block("Infinite Health", {0x20123456U, 0x00000063U}));
    return blocks;
}

void require_words(const formats::text::CheatBlock& block,
                   std::initializer_list<std::uint32_t> expected,
                   std::string_view message) {
    require(block.cheat.words == std::vector<std::uint32_t>(expected), message);
}

void test_armax_bin_round_trip() {
    auto blocks = sample_blocks();
    blocks.erase(blocks.begin() + 1); // AR MAX writer excludes empty organizers.
    blocks[0].cheat.words = {0x00000000U, 0x08000000U,
                             0xC4145714U, 0x0003FF00U};
    blocks[1].cheat.words = {0x04001234U, 0x00000063U};

    formats::binary::WriteOptions options;
    options.game_name = "Binary Test";
    options.game_id = 0x123U;
    const auto bytes = formats::binary::write_armax_bin(blocks, options);
    const auto loaded = formats::binary::load_armax_bin(bytes);

    require(loaded.game_name == "Binary Test", "AR MAX BIN lost the game name");
    require(loaded.game_id == 0x123U, "AR MAX BIN lost the game ID");
    require(loaded.input_format == CodeFormat::action_replay_max,
            "AR MAX BIN selected the wrong input format");
    require(loaded.blocks.size() == 2U, "AR MAX BIN changed the cheat count");
    require(loaded.blocks[0].cheat.flags[Cheat::flag_master_code] != 0U,
            "AR MAX BIN lost the master-code flag");
    require(loaded.blocks[0].cheat.id == 0U,
            "AR MAX BIN did not preserve the serialized cheat ID");
    require(loaded.blocks[0].cheat.flags[Cheat::flag_comments] == 1U,
            "AR MAX BIN did not preserve the legacy comment flag");
    require_words(loaded.blocks[0],
                  {0x00000000U, 0x08000000U, 0xC4145714U, 0x0003FF00U},
                  "AR MAX BIN changed master-code words");
    require_words(loaded.blocks[1], {0x04001234U, 0x00000063U},
                  "AR MAX BIN changed normal-code words");
}

void test_cbc_round_trip(formats::binary::CbcVersion version) {
    const auto blocks = sample_blocks();
    formats::binary::WriteOptions options;
    options.game_name = version == formats::binary::CbcVersion::v8
                            ? "CBC V8 Test"
                            : "CBC V7 Test";
    options.cbc_version = version;
    const auto bytes = formats::binary::write_cbc(blocks, options);
    const auto loaded = formats::binary::load_cbc(bytes);

    require(loaded.game_name == options.game_name, "CBC lost the game name");
    require(loaded.blocks.size() == blocks.size(), "CBC changed the entry count");
    require(loaded.blocks[1].cheat.empty(), "CBC lost an organizer heading");
    require(loaded.blocks[1].label == "Player Codes", "CBC changed a heading name");
    require_words(loaded.blocks[0], {0xF0145714U, 0x00145717U},
                  "CBC changed master-code words");
    require_words(loaded.blocks[2], {0x20123456U, 0x00000063U},
                  "CBC changed normal-code words");
    const CodeFormat expected = version == formats::binary::CbcVersion::v8
                                    ? CodeFormat::codebreaker7_common
                                    : CodeFormat::codebreaker1_6;
    require(loaded.input_format == expected, "CBC selected the wrong input format");
}

void test_p2m_round_trip() {
    auto blocks = sample_blocks();
    formats::binary::WriteOptions options;
    options.game_name = "P2M Test";
    const auto bytes = formats::binary::write_p2m(blocks, options);
    const auto loaded = formats::binary::load_p2m(bytes);

    require(loaded.game_name == "P2M Test", "P2M lost the game name");
    require(loaded.input_format == CodeFormat::gameshark_madcatz5,
            "P2M selected the wrong input format");
    require(loaded.blocks.size() == blocks.size(), "P2M changed the entry count");
    require(loaded.blocks[0].label == "{5(M)", "P2M did not preserve the master marker");
    require(loaded.blocks[1].label == "{3Player Codes",
            "P2M did not preserve the organizer marker");
    require_words(loaded.blocks[2], {0x20123456U, 0x00000063U},
                  "P2M changed normal-code words");
}


void test_utf16_binary_text_round_trip() {
    constexpr std::string_view armax_game_name = "Café テスト";
    constexpr std::string_view armax_cheat_name = "無限HP";

    auto blocks = sample_blocks();
    blocks.erase(blocks.begin() + 1);
    blocks[0].label = std::string(armax_cheat_name);
    blocks[0].cheat.name = std::string(armax_cheat_name);
    blocks[0].cheat.words = {0x00000000U, 0x08000000U,
                             0xC4145714U, 0x0003FF00U};
    blocks[1].cheat.words = {0x04001234U, 0x00000063U};

    formats::binary::WriteOptions options;
    options.game_name = std::string(armax_game_name);
    options.game_id = 0x123U;

    const auto armax = formats::binary::load_armax_bin(
        formats::binary::write_armax_bin(blocks, options));
    require(armax.game_name == armax_game_name,
            "AR MAX BIN changed a Unicode game name");
    require(armax.blocks[0].label == armax_cheat_name,
            "AR MAX BIN changed a Unicode cheat name");

    constexpr std::string_view p2m_game_name = "Café テスト 😀";
    constexpr std::string_view p2m_cheat_name = "無限HP 😀";
    options.game_name = std::string(p2m_game_name);
    blocks[0].label = std::string(p2m_cheat_name);
    blocks[0].cheat.name = std::string(p2m_cheat_name);
    const auto p2m = formats::binary::load_p2m(
        formats::binary::write_p2m(blocks, options));
    require(p2m.game_name == p2m_game_name,
            "P2M changed a Unicode game name");
    require(p2m.blocks[0].label == std::string("{5") + std::string(p2m_cheat_name),
            "P2M changed a Unicode cheat name");
}

void test_swap_magic_layout_and_crc() {
    auto blocks = sample_blocks();
    formats::binary::WriteOptions options;
    options.game_name = "Swap Test";
    const auto bytes = formats::binary::write_swap_magic_bin(blocks, options);
    require(bytes.size() > 20U, "Swap Magic BIN output is too small");

    formats::binary::common::Reader reader(bytes);
    const std::uint32_t stored_crc = reader.u32le();
    const std::uint32_t data_size = reader.u32le();
    require(reader.u32le() == 1U, "Swap Magic BIN game count is not one");
    require(reader.u32le() == 2U, "Swap Magic BIN active cheat count is incorrect");
    require(reader.u32le() == 2U, "Swap Magic BIN total line count is incorrect");
    require(data_size == bytes.size() - 20U, "Swap Magic BIN data size is incorrect");
    const std::vector<std::uint8_t> payload(bytes.begin() + 20, bytes.end());
    const std::uint32_t expected_crc =
        0x5FA27ED6U ^ devices::gameshark::file_crc32(payload);
    require(stored_crc == expected_crc, "Swap Magic BIN CRC is incorrect");

    const auto loaded = formats::binary::load_swap_magic_bin(bytes);
    require(loaded.game_name == "Swap Test", "Swap Magic BIN lost the game name");
    require(loaded.input_format == CodeFormat::swap_magic_coder3,
            "Swap Magic BIN selected the wrong input format");
    require(loaded.blocks.size() == 2U, "Swap Magic BIN changed the active cheat count");
    require(loaded.blocks[0].cheat.flags[Cheat::flag_master_code] != 0U,
            "Swap Magic BIN did not restore the first-entry master flag");
    require_words(loaded.blocks[0], {0xF0145714U, 0x00145717U},
                  "Swap Magic BIN changed master-code words");
    require_words(loaded.blocks[1], {0x20123456U, 0x00000063U},
                  "Swap Magic BIN changed normal-code words");

    auto corrupt = bytes;
    corrupt.back() ^= 0x01U;
    bool crc_rejected = false;
    try {
        static_cast<void>(formats::binary::load_swap_magic_bin(corrupt));
    } catch (const std::runtime_error&) {
        crc_rejected = true;
    }
    require(crc_rejected, "Swap Magic BIN reader accepted an invalid payload CRC");
}


void test_cbc_reference_headers() {
    const auto blocks = sample_blocks();

    formats::binary::WriteOptions v7_options;
    v7_options.game_name = "CBC Header Test";
    v7_options.cbc_version = formats::binary::CbcVersion::v7;
    const auto v7 = formats::binary::write_cbc(blocks, v7_options);
    require(v7.size() > 64U, "CBC v7 output has no encrypted payload");
    require(std::string(v7.begin(), v7.begin() + 15) == "CBC Header Test",
            "CBC v7 title header does not match OmniConvertCS");
    require(std::all_of(v7.begin() + 15, v7.begin() + 64,
                        [](std::uint8_t value) { return value == 0U; }),
            "CBC v7 title header was not zero padded");

    formats::binary::WriteOptions v8_options = v7_options;
    v8_options.cbc_version = formats::binary::CbcVersion::v8;
    const auto v8 = formats::binary::write_cbc(blocks, v8_options);
    formats::binary::common::Reader reader(v8);
    require(reader.u32le() == 0x01554643U, "CBC v8 CFU file ID is incorrect");
    constexpr std::string_view banner = "Created with OmniconvertCS CBC v8 ";
    for (std::size_t index = 0U; index < 256U; ++index) {
        require(v8[4U + index] == static_cast<std::uint8_t>(banner[index % banner.size()]),
                "CBC v8 signature banner differs from OmniConvertCS");
    }
    reader.seek(260U);
    require(reader.u32le() == 0x00000800U, "CBC v8 version field is incorrect");
    require(reader.fixed_ascii(72U) == "CBC Header Test",
            "CBC v8 game title field is incorrect");
    require(reader.u32le() == 344U, "CBC v8 data offset is incorrect");
    require(reader.u32le() == 0U, "CBC v8 reserved field is not zero");
}

void test_p2m_reference_compatibility() {
    const auto blocks = sample_blocks();
    formats::binary::WriteOptions options;
    options.game_name = std::string(80U, 'A');
    auto bytes = formats::binary::write_p2m(blocks, options);

    require(bytes.size() > 0x110U, "P2M reference output is unexpectedly small");
    require(bytes[8U + 55U] == 0U,
            "P2M fixed title field did not reserve its terminating NUL byte");
    require(std::all_of(bytes.begin() + 8, bytes.begin() + 8 + 55,
                        [](std::uint8_t value) { return value == static_cast<std::uint8_t>('A'); }),
            "P2M fixed title field was truncated at the wrong length");

    // Official and third-party archives are inconsistent about filename case.
    // Match the C# reader's ordinal-ignore-case lookup for user.dat.
    constexpr std::size_t user_name_offset = 0xF0U;
    constexpr std::string_view uppercase_name = "USER.DAT";
    std::copy(uppercase_name.begin(), uppercase_name.end(),
              bytes.begin() + static_cast<std::ptrdiff_t>(user_name_offset));
    const auto loaded = formats::binary::load_p2m(bytes);
    require(loaded.blocks.size() == blocks.size(),
            "P2M reader did not accept a case-variant USER.DAT descriptor");
}

void test_armax_reference_header() {
    auto blocks = sample_blocks();
    blocks.erase(blocks.begin() + 1);
    blocks[0].cheat.words = {0x00000000U, 0x08000000U,
                             0xC4145714U, 0x0003FF00U};
    blocks[1].cheat.words = {0x04001234U, 0x00000063U};
    blocks[0].cheat.id = 0x111U;
    blocks[1].cheat.id = 0x222U;

    formats::binary::WriteOptions options;
    options.game_name = "ARMAX Header Test";
    options.game_id = 0x123U;
    const auto bytes = formats::binary::write_armax_bin(blocks, options);
    formats::binary::common::Reader reader(bytes);
    require(reader.fixed_ascii(12U) == "PS2_CODELIST",
            "AR MAX BIN list identifier is incorrect");
    require(reader.u32le() == 0xD00DB00BU, "AR MAX BIN list version is incorrect");
    require(reader.u32le() == 0U, "AR MAX BIN unknown header field is not zero");
    const std::uint32_t list_size = reader.u32le();
    const std::uint32_t stored_crc = reader.u32le();
    require(reader.u32le() == 1U, "AR MAX BIN game count is incorrect");
    require(reader.u32le() == 2U, "AR MAX BIN code count is incorrect");
    require(list_size == bytes.size() - 28U, "AR MAX BIN list-size field is incorrect");
    require(stored_crc == formats::binary::common::crc32_standard(bytes, 28U, list_size),
            "AR MAX BIN header CRC is incorrect");

    const auto loaded = formats::binary::load_armax_bin(bytes);
    require(loaded.blocks[0].cheat.id == 0x111U &&
                loaded.blocks[1].cheat.id == 0x222U,
            "AR MAX BIN did not preserve serialized cheat IDs");
}

void test_conversion_result_keeps_binary_metadata() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::action_replay_max;
    request.transpose_mode = translate::TransposeMode::original;
    request.armax_game_id = 0x123U;

    const convert::Result result = convert::convert_text(
        "(M)\nF0145714 00145717\nInfinite Health\n20123456 00000063\n",
        request);
    require(result.blocks.size() == 2U,
            "Conversion result did not retain converted cheat blocks");
    require(result.blocks[0].cheat.flags[Cheat::flag_master_code] != 0U,
            "Conversion result did not retain the master-code flag");
    require(!result.blocks[0].cheat.empty(),
            "Conversion result did not retain encrypted master words");
}

void test_binary_validation() {
    auto blocks = sample_blocks();
    blocks[0].cheat.flags[Cheat::flag_master_code] = 0U;
    bool rejected = false;
    try {
        (void)formats::binary::write_cbc(blocks, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "CBC export accepted a list without a master code");

    require(formats::binary::application_format_name(
                formats::binary::ApplicationFormat::swap_magic_bin) ==
                "Swap Magic BIN",
            "Binary application format names are incomplete");
}

} // namespace

void run_binary_format_tests() {
    test_armax_bin_round_trip();
    test_cbc_round_trip(formats::binary::CbcVersion::v7);
    test_cbc_round_trip(formats::binary::CbcVersion::v8);
    test_p2m_round_trip();
    test_utf16_binary_text_round_trip();
    test_cbc_reference_headers();
    test_p2m_reference_compatibility();
    test_armax_reference_header();
    test_swap_magic_layout_and_crc();
    test_conversion_result_keeps_binary_metadata();
    test_binary_validation();
}

} // namespace omni::tests
