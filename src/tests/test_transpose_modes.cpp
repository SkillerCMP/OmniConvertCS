#include "tests/test_transpose_modes.hpp"

#include "convert/conversion_service.hpp"
#include "core/cheat.hpp"
#include "tests/test_support.hpp"
#include "translate/translation_error.hpp"
#include "translate/translator.hpp"
#include "translate/transpose_mode.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace omni::tests {
namespace {

void require_words(const Cheat& cheat,
                   std::initializer_list<std::uint32_t> expected,
                   std::string_view message) {
    require(cheat.words.size() == expected.size(), message);
    std::size_t index = 0U;
    for (const std::uint32_t value : expected) {
        require(cheat.words[index] == value, message);
        ++index;
    }
}


std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "transpose-mode vector file could not be opened");
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

std::vector<std::string> lines_with_prefix(std::string_view text,
                                           std::string_view prefix) {
    std::vector<std::string> lines;
    std::istringstream input{std::string(text)};
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0U) == 0U) lines.push_back(std::move(line));
    }
    return lines;
}

void require_translation_error(const std::function<void()>& action,
                               translate::ErrorCode expected,
                               std::string_view message) {
    try {
        action();
    } catch (const translate::TranslationError& error) {
        require(error.code() == expected, message);
        return;
    }
    throw std::runtime_error(std::string(message));
}

void test_default_mode_is_original() {
    const convert::Request request;
    require(request.transpose_mode == translate::TransposeMode::original,
            "conversion requests should default to Original transpose mode");
}

void test_mode_parser() {
    require(translate::parse_transpose_mode("strict") ==
                translate::TransposeMode::strict,
            "STRICT transpose mode was not parsed");
    require(translate::parse_transpose_mode("Original") ==
                translate::TransposeMode::original,
            "ORIGINAL transpose mode was not parsed");
    require(!translate::parse_transpose_mode("unknown"),
            "unknown transpose mode should be rejected");
}

void test_noncanonical_type_f_difference() {
    Cheat strict;
    strict.words = {0xF0145714U, 0x0000000EU};
    require_translation_error(
        [&]() {
            translate::translate_cheat(
                strict, Device::ar2, Device::armax, nullptr,
                translate::TransposeMode::strict);
        },
        translate::ErrorCode::unsupported_hook,
        "Strict mode should reject a noncanonical standard type-F hook");

    Cheat original;
    original.words = {0xF0145714U, 0x0000000EU};
    translate::translate_cheat(original, Device::ar2, Device::armax, nullptr,
                               translate::TransposeMode::original);
    require_words(original, {0xC4145714U, 0x0003FF00U},
                  "Original mode did not reproduce the C# broad type-F mapping");
}

void test_entrypoint_interrupt_hook() {
    Cheat original;
    original.words = {0xF0100000U, 0x0000000EU};
    translate::translate_cheat(original, Device::ar2, Device::armax, nullptr,
                               translate::TransposeMode::original);
    require_words(original, {0xC4100000U, 0x0002FF01U},
                  "Original mode did not reproduce the C# entry-point interrupt hook mapping");
}

void test_armax_template_difference() {
    Cheat strict;
    strict.words = {0xC4145714U, 0xDEADBEEFU};
    require_translation_error(
        [&]() {
            translate::translate_cheat(
                strict, Device::armax, Device::ar2, nullptr,
                translate::TransposeMode::strict);
        },
        translate::ErrorCode::unsupported_hook,
        "Strict mode should reject a nonstandard ARMAX hook template");

    Cheat original;
    original.words = {0xC4145714U, 0xDEADBEEFU};
    translate::translate_cheat(original, Device::armax, Device::ar2, nullptr,
                               translate::TransposeMode::original);
    require_words(original, {0xF0145714U, 0x00145717U},
                  "Original mode did not reproduce the C# C4 hook reconstruction");
}

void test_canonical_hook_matches_both_modes() {
    for (const translate::TransposeMode mode : {
             translate::TransposeMode::strict,
             translate::TransposeMode::original}) {
        Cheat cheat;
        cheat.words = {0xF0145714U, 0x00145717U};
        translate::translate_cheat(cheat, Device::ar2, Device::armax,
                                   nullptr, mode);
        require_words(cheat, {0xC4145714U, 0x0003FF00U},
                      "canonical type-F hook should map in both modes");
    }
}

void test_conversion_request_uses_original_mode() {
    convert::Request request;
    request.input_format = CodeFormat::ar12_raw;
    request.output_format = CodeFormat::armax_raw;
    request.transpose_mode = translate::TransposeMode::original;

    const convert::Result result = convert::convert_text(
        "Legacy master, CRYPT_ARAW\n"
        "F0145714 0000000E\n",
        request);

    require(result.output_text.find("C4145714 0003FF00") !=
                std::string::npos,
            "conversion request did not route through Original transpose mode");
}


void test_original_mode_commits_partial_result_on_error() {
    Cheat original;
    original.words = {0x20A07640U, 0x00000063U};
    require_translation_error(
        [&]() {
            translate::translate_cheat(
                original, Device::armax, Device::standard, nullptr,
                translate::TransposeMode::original);
        },
        translate::ErrorCode::test_size,
        "Original mode should report the legacy byte-comparison error");
    require(original.empty(),
            "Original mode did not commit the C# translator's empty partial result");
}

void test_full_armax_collection_to_pnach_original() {
    const std::filesystem::path vectors = OMNI_TEST_VECTOR_DIR;
    const std::string encrypted = read_text_file(
        vectors / "ARMAX[ENCRYPTED]-.hack__G.U. Vol 1_ Rebirth.txt");
    const std::string expected = read_text_file(
        vectors / "ARMAX[EXPECTED-PNACH-ORIGINAL]-.hack__G.U. Vol 1_ Rebirth.txt");

    convert::Request request;
    request.input_format = CodeFormat::action_replay_max;
    request.output_format = CodeFormat::pnach_raw;
    request.transpose_mode = translate::TransposeMode::original;

    const convert::Result result = convert::convert_text(encrypted, request);
    require(!result.warnings.empty(),
            "Original collection conversion should retain the legacy organizer warning");
    require(result.output_text.find("patch=1,EE,F0144134,extended,00144137") !=
                std::string::npos,
            "ARMAX master hook did not convert through Original mode");
    require(result.output_text.find(
                "[Infinite Chim by Codejunkies\\Extra Characters Enabled]") !=
                std::string::npos,
            "legacy failed parent entry was not retained as a PNACH organizer");
    require(result.output_text.find(
                "[Haseo Codes by Codejunkies\\Max Level]") !=
                std::string::npos,
            "ARMAX verifier-only folder was not retained as a PNACH organizer");
    require(result.output_text.find("// [ERROR]") == std::string::npos,
            "empty legacy organizer should not become a PNACH error block");

    const auto actual_patches = lines_with_prefix(result.output_text, "patch=");
    const auto expected_patches = lines_with_prefix(expected, "patch=");
    require(actual_patches == expected_patches,
            "full ARMAX-to-PNACH Original conversion differs from the C# patch sequence");
}

} // namespace

void run_transpose_mode_tests() {
    test_default_mode_is_original();
    test_mode_parser();
    test_noncanonical_type_f_difference();
    test_entrypoint_interrupt_hook();
    test_armax_template_difference();
    test_canonical_hook_matches_both_modes();
    test_conversion_request_uses_original_mode();
    test_original_mode_commits_partial_result_on_error();
    test_full_armax_collection_to_pnach_original();
}

} // namespace omni::tests
