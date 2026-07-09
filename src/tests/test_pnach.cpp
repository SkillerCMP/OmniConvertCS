#include "tests/test_pnach.hpp"

#include "convert/conversion_service.hpp"
#include "formats/pnach/pnach_parser.hpp"
#include "formats/pnach/pnach_writer.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/test_support.hpp"

#include <string>
#include <string_view>

namespace omni::tests {
namespace {

constexpr std::string_view sample_pnach =
    "gametitle=Sample Game\n"
    "comment=Example\n"
    "crc=12345678\n"
    "\n"
    "[Player\\Infinite Health]\n"
    "author=CODE JUNKIES\n"
    "patch=1,EE,20123456,extended,DEADBEEF\n"
    "patch=1,EE,1012345A,extended,0000????\n"
    "\n"
    "[Player\\Infinite Ammo]\n"
    "patch=1,EE,20123460,extended,00000063\n";

void test_format_identity() {
    require(parse_code_format("PNACH") == CodeFormat::pnach_raw,
            "PNACH format token was not recognized");
    require(parse_code_format("Pnach(RAW)") == CodeFormat::pnach_raw,
            "Pnach(RAW) alias was not recognized");
    require(crypt_for_format(CodeFormat::pnach_raw) == Crypt::raw,
            "PNACH should use Standard RAW crypto");
    require(device_for_format(CodeFormat::pnach_raw) == Device::standard,
            "PNACH should use the Standard device semantics");
}

void test_parse_groups_authors_and_wildcards() {
    const auto blocks = formats::pnach::parse_text(sample_pnach);
    require(blocks.size() == 4U, "PNACH group/section block count is incorrect");
    require(blocks[0].kind == formats::text::TextBlockKind::cmp_group_open &&
                blocks[0].label && *blocks[0].label == "Player" &&
                blocks[0].cheat.empty(),
            "PNACH group was not represented as an explicit organizer block");
    require(blocks[1].label && *blocks[1].label == "Infinite Health by Code Junkies",
            "PNACH author normalization did not match the C# behavior");
    require(blocks[1].cheat.words.size() == 4U,
            "PNACH patch rows were not attached to the section");
    require(blocks[1].cheat.words[0] == 0x20123456U &&
                blocks[1].cheat.words[1] == 0xDEADBEEFU,
            "PNACH first patch row parsed incorrectly");
    require(blocks[1].wildcards.size() == 1U &&
                blocks[1].wildcards[0].word_index == 2U,
            "PNACH wildcard mask was not retained");
    require(blocks[2].label && *blocks[2].label == "Infinite Ammo",
            "PNACH second section name parsed incorrectly");
    require(blocks[3].kind == formats::text::TextBlockKind::cmp_group_close,
            "PNACH parser did not close the open group path");
}

void test_write_groups_authors_and_crc() {
    const auto blocks = formats::pnach::parse_text(sample_pnach);
    formats::pnach::WriteOptions options;
    options.game_name = "Sample Game";
    options.include_crc = true;
    options.crc = 0x12345678U;
    const std::string output = formats::pnach::write_text(blocks, options);

    require(output.find("// GameName: Sample Game\n// CRC: 12345678\n") != std::string::npos,
            "PNACH CRC header was not emitted");
    require(output.find("[Player\\Infinite Health]\nauthor=Code Junkies\n") != std::string::npos,
            "PNACH group/author output is incorrect");
    require(output.find("patch=1,EE,1012345A,extended,0000????\n") != std::string::npos,
            "PNACH wildcard output was not preserved");
}


void test_old_mac_line_endings() {
    const std::string input =
        "[Player\\Infinite Health]\r"
        "author=Skiller\r"
        "patch=1,EE,20123456,extended,DEADBEEF\r";
    const auto blocks = formats::pnach::parse_text(input);
    require(blocks.size() == 3U && blocks[1].cheat.word_count() == 2U,
            "standalone CR PNACH input collapsed into one line");
    require(blocks[1].label && *blocks[1].label == "Infinite Health by Skiller",
            "standalone CR PNACH metadata was not parsed");
}

void test_conversion_service_routing() {
    convert::Request to_raw;
    to_raw.input_format = CodeFormat::pnach_raw;
    to_raw.output_format = CodeFormat::standard_raw;
    const convert::Result raw = convert::convert_text(sample_pnach, to_raw);
    require(raw.output_text.find("Infinite Health by Code Junkies\n") != std::string::npos,
            "PNACH input did not route through the PNACH parser");
    require(raw.output_text.find("20123456 DEADBEEF\n") != std::string::npos,
            "PNACH input did not become Standard RAW rows");

    convert::Request to_pnach;
    to_pnach.input_format = CodeFormat::standard_raw;
    to_pnach.output_format = CodeFormat::pnach_raw;
    to_pnach.game_name = "Output Game";
    const convert::Result pnach = convert::convert_text(
        "Player\nInfinite Health by Skiller\n20123456 DEADBEEF\n", to_pnach);
    require(pnach.output_text.find("[Player\\Infinite Health]\nauthor=Skiller\n") != std::string::npos,
            "Standard RAW output did not route through the PNACH writer");
    require(pnach.output_text.find("patch=1,EE,20123456,extended,DEADBEEF\n") != std::string::npos,
            "Standard RAW did not become a PNACH patch row");
}

void test_pnach_paths_become_cmp_nested_groups() {
    const std::string input =
        "[Inventory Pointer Capture\\Have Camouflage Face Codes\\Infinity]\n"
        "author=Code Master\n"
        "description=Enable Code (Must Be On)\n"
        "patch=1,EE,600C0000,extended,00000001\n"
        "patch=1,EE,00010001,extended,00002FC8\n";

    convert::Request request;
    request.input_format = CodeFormat::pnach_raw;
    request.output_format = CodeFormat::standard_raw;
    request.cmp_output_mode = true;

    const convert::Result result = convert::convert_text(input, request);
    const std::string expected =
        "!Inventory Pointer Capture:\n"
        "!Have Camouflage Face Codes:\n"
        "+Infinity\n"
        "%Credits: Code Master\n"
        "$600C0000 00000001\n"
        "$00010001 00002FC8\n"
        "!!\n"
        "!!\n";
    require(result.output_text == expected,
            "PNACH backslash path was not converted to nested CMP groups");
}

} // namespace

void run_pnach_tests() {
    test_format_identity();
    test_parse_groups_authors_and_wildcards();
    test_write_groups_authors_and_crc();
    test_old_mac_line_endings();
    test_conversion_service_routing();
    test_pnach_paths_become_cmp_nested_groups();
}

} // namespace omni::tests
