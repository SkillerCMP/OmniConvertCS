#include "tests/test_cmp_output_mode.hpp"

#include "convert/conversion_service.hpp"
#include "tests/test_support.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace omni::tests {
namespace {

constexpr std::string_view sample_raw =
    "Infinite Health\n"
    "20123456 DEADBEEF\n"
    "20123458 00000001\n";

void test_cmp_markers_for_standard_text_output() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::standard_raw;
    request.cmp_output_mode = true;

    const convert::Result result = convert::convert_text(sample_raw, request);
    const std::string expected =
        "+Infinite Health\n"
        "$20123456 DEADBEEF\n"
        "$20123458 00000001\n";
    require(result.output_text == expected,
            "CMP Output Mode did not add + labels and $ code rows");
}

void test_cmp_markers_for_armax_text_output() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::action_replay_max;
    request.armax_game_id = 0x0357U;
    request.armax_verifier_nonce = 0x12345U;
    request.cmp_output_mode = true;

    const convert::Result result = convert::convert_text(
        "Infinite Health\n20123456 DEADBEEF\n", request);
    std::istringstream lines(result.output_text);
    std::string line;
    require(static_cast<bool>(std::getline(lines, line)) &&
                line.rfind("+Infinite Health", 0U) == 0U,
            "CMP mode did not mark the ARMAX code name");
    std::size_t code_lines = 0U;
    while (std::getline(lines, line)) {
        if (line.empty()) continue;
        require(line.front() == '$',
                "CMP mode did not mark an ARMAX encrypted code row");
        ++code_lines;
    }
    require(code_lines >= 2U,
            "ARMAX CMP output did not include verifier and payload rows");
}

void test_cmp_group_sections_and_inline_credits() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::standard_raw;
    request.cmp_output_mode = true;

    const std::string input =
        "Master Codes\n"
        "Enable Code (Must Be On) [Read Note] , by Lajos Szalay , Crypt Codebreaker v7+\n"
        "904903A8 0C109375\n"
        "2012EF74 00441021\n"
        "Activate Cheat Codes\n";

    const convert::Result result = convert::convert_text(input, request);
    const std::string expected =
        "!Master Codes:\n"
        "+Enable Code (Must Be On) [Read Note] , Crypt Codebreaker v7+\n"
        "%Credits: Lajos Szalay\n"
        "$904903A8 0C109375\n"
        "$2012EF74 00441021\n"
        "!!\n"
        "!Activate Cheat Codes:\n"
        "!!\n";
    require(result.output_text == expected,
            "CMP mode did not create/close groups or move inline credits");
}

void test_cmp_explicit_close_ends_the_group_before_following_code() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::standard_raw;
    request.cmp_output_mode = true;

    const std::string input =
        "!Master Codes:\n"
        "+Enable Code by Lajos Szalay , CRYPT_RAW\n"
        "$904903A8 0C109375\n"
        "!!\n"
        "+Outside Group\n"
        "$2012EF74 00441021\n";

    const convert::Result result = convert::convert_text(input, request);
    const std::string expected =
        "!Master Codes:\n"
        "+Enable Code , CRYPT_RAW\n"
        "%Credits: Lajos Szalay\n"
        "$904903A8 0C109375\n"
        "!!\n"
        "+Outside Group\n"
        "$2012EF74 00441021\n";
    require(result.output_text == expected,
            "CMP !! did not close the preceding group exactly once");
}

void test_cmp_next_group_closes_the_previous_group() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::standard_raw;
    request.cmp_output_mode = true;

    const convert::Result result = convert::convert_text(
        "First Group\nSecond Group\n", request);
    const std::string expected =
        "!First Group:\n"
        "!!\n"
        "!Second Group:\n"
        "!!\n";
    require(result.output_text == expected,
            "CMP group transition did not close the group before it");
}

void test_normal_text_output_is_unchanged() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::standard_raw;

    const convert::Result result = convert::convert_text(sample_raw, request);
    require(result.output_text.find("+Infinite Health") == std::string::npos,
            "normal output unexpectedly gained a CMP + label marker");
    require(result.output_text.find("$20123456") == std::string::npos,
            "normal output unexpectedly gained a CMP $ code marker");
}

void test_pnach_ignores_cmp_mode() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::pnach_raw;
    request.cmp_output_mode = true;

    const convert::Result result = convert::convert_text(sample_raw, request);
    require(result.output_text.find("patch=1,EE,20123456,extended,DEADBEEF") !=
                std::string::npos,
            "PNACH output was not generated while CMP mode was enabled");
    require(result.output_text.find("+Infinite Health") == std::string::npos &&
                result.output_text.find("$20123456") == std::string::npos,
            "CMP markers leaked into PNACH output");
}

void test_mips_ignores_cmp_mode() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::ps2_mips_r5900;
    request.cmp_output_mode = true;

    const convert::Result result = convert::convert_text(
        "Code Cave\n200A0000 03E00008\n200A0004 00000000\n", request);
    require(result.output_text.find("jr $ra") != std::string::npos,
            "MIPS output was not generated while CMP mode was enabled");
    require(result.output_text.find("+Code Cave") == std::string::npos &&
                result.output_text.find("$200A0000") == std::string::npos,
            "CMP markers leaked into MIPS output");
}

} // namespace

void run_cmp_output_mode_tests() {
    test_cmp_markers_for_standard_text_output();
    test_cmp_markers_for_armax_text_output();
    test_cmp_group_sections_and_inline_credits();
    test_cmp_explicit_close_ends_the_group_before_following_code();
    test_cmp_next_group_closes_the_previous_group();
    test_normal_text_output_is_unchanged();
    test_pnach_ignores_cmp_mode();
    test_mips_ignores_cmp_mode();
}

} // namespace omni::tests
