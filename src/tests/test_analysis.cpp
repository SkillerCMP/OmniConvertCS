#include "tests/test_analysis.hpp"

#include "analysis/e001_fixer.hpp"
#include "analysis/raw_plausibility.hpp"
#include "convert/conversion_service.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "devices/action_replay/ar_batch.hpp"
#include "tests/test_support.hpp"
#include "util/hex.hpp"

#include <string>
#include <vector>

namespace omni::tests {
namespace {

void test_e001_rewrite() {
    const std::string input =
        "Keep\r\n"
        "E0011234 001ABCDE\r\n"
        "20100000 DEADBEEF\n";
    const std::string output = analysis::rewrite_e001_to_d(input);
    require(output.find("D01ABCDE 00001234") != std::string::npos,
            "E001 conditional rewrite did not match OmniConvertCS");
    require(output.find("20100000 DEADBEEF") != std::string::npos,
            "E001 conditional rewrite changed an unrelated row");
}

void test_raw_plausibility() {
    Cheat valid;
    valid.words = {0x20100000U, 0xDEADBEEFU};
    const auto valid_result = analysis::validate_raw_plausibility(
        valid, Crypt::raw, analysis::AnalyzeMode::analyze);
    require(valid_result.valid,
            "RAW plausibility rejected a normal aligned 32-bit write");

    Cheat invalid;
    invalid.words = {0xB0100000U, 0x00000001U};
    const auto invalid_result = analysis::validate_raw_plausibility(
        invalid, Crypt::codebreaker_raw, analysis::AnalyzeMode::analyze_fix);
    require(!invalid_result.valid && invalid_result.rule == "TypeBNotAllowed",
            "RAW plausibility did not reject an invalid B-type record");

    Cheat cb7;
    cb7.words = {0xFE123456U, 0x00000000U,
                 0x000FFFFEU, 0x00000000U,
                 0x20100000U, 0x12345678U};
    const auto cb7_result = analysis::validate_raw_plausibility(
        cb7, Crypt::codebreaker_raw, analysis::AnalyzeMode::analyze);
    require(cb7_result.valid && !cb7_result.note.empty(),
            "RAW plausibility did not skip the CB7 master prologue");
}

void test_per_code_error_blocks_continue() {
    const std::string input =
        "Bad Hook\n"
        "F0145714 0000000E\n"
        "Good Code\n"
        "20100000 DEADBEEF\n";

    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::armax_raw;
    request.transpose_mode = translate::TransposeMode::strict;
    request.continue_on_error = true;
    const convert::Result result = convert::convert_text(input, request);

    require(result.issues.size() == 1U,
            "Per-code continuation did not record exactly one failed cheat");
    require(result.issues.front().cheat_name == "Bad Hook",
            "Per-code continuation reported the wrong failed cheat");
    require(result.output_text.find("// [ERROR] Bad Hook") != std::string::npos,
            "GUI-style conversion did not emit an error block");
    require(result.output_text.find("Good Code") != std::string::npos,
            "Per-code continuation discarded a later valid cheat");
    require(result.output_text.find("C4100000") != std::string::npos ||
                result.output_text.find("04100000") != std::string::npos,
            "Later valid cheat was not translated after a failed cheat");
}


void test_ar2_key_survives_later_translation_error() {
    using devices::action_replay::Context;
    constexpr std::uint32_t key_seed = 0x04030209U;

    std::vector<std::uint32_t> first{0xF0145714U, 0x0000000EU};
    devices::action_replay::prepend_ar2_key_code(first, key_seed);
    Context encrypt_context;
    encrypt_context.encrypt_ar2(first);

    std::vector<std::uint32_t> second{0x20100000U, 0xDEADBEEFU};
    encrypt_context.encrypt_ar2(second);

    const auto row = [](std::uint32_t a, std::uint32_t v) {
        return hex::format_u32(a) + " " + hex::format_u32(v) + "\n";
    };
    std::string input = "Bad keyed hook\n";
    for (std::size_t i = 0; i + 1U < first.size(); i += 2U) {
        input += row(first[i], first[i + 1U]);
    }
    input += "Later keyed code\n";
    input += row(second[0], second[1]);

    convert::Request request;
    request.input_format = CodeFormat::action_replay2;
    request.output_format = CodeFormat::armax_raw;
    request.transpose_mode = translate::TransposeMode::strict;
    request.continue_on_error = true;
    const convert::Result result = convert::convert_text(input, request);

    require(result.issues.size() == 1U,
            "AR2 keyed translation-error regression did not isolate one failed block");
    require(result.output_text.find("Later keyed code") != std::string::npos &&
                result.output_text.find("04100000 DEADBEEF") != std::string::npos,
            "AR2 key state was lost after an earlier block failed translation");
    require(result.active_ar2_seed == key_seed,
            "AR2 indicator did not retain the key after a translated block failed");
}

void test_active_ar2_key_is_reported() {
    const std::string encrypted =
        "(M)\n"
        "0E3C7DF2 1853E59E\n"
        "EE8AA78A BCBDF29A\n";
    convert::Request request;
    request.input_format = CodeFormat::action_replay2;
    request.output_format = CodeFormat::ar12_raw;
    const convert::Result result = convert::convert_text(encrypted, request);
    require(result.active_ar2_seed.has_value(),
            "AR2 conversion did not report the active key seed");
    require(*result.active_ar2_seed == 0x04030209U,
            "AR2 active key indicator returned the wrong embedded key");
    require(devices::action_replay::encrypt_word(*result.active_ar2_seed,
                                                  0x05U, 0x18U) == 0x1853E59EU,
            "AR2 active seed does not reproduce the displayed encrypted key");
}

} // namespace

void run_analysis_tests() {
    test_e001_rewrite();
    test_raw_plausibility();
    test_per_code_error_blocks_continue();
    test_active_ar2_key_is_reported();
    test_ar2_key_survives_later_translation_error();
}

} // namespace omni::tests
