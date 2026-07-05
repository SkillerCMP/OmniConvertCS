#include "tests/test_inline_mode.hpp"

#include "convert/conversion_service.hpp"
#include "core/code_format.hpp"
#include "devices/action_replay/ar_batch.hpp"
#include "tests/crypto/vector_file.hpp"
#include "tests/test_support.hpp"
#include "translate/transpose_mode.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <vector>

#ifndef OMNI_TEST_VECTOR_DIR
#error OMNI_TEST_VECTOR_DIR must be defined for INLINE vector tests
#endif

namespace omni::tests {
namespace {

const std::filesystem::path vector_root{OMNI_TEST_VECTOR_DIR};

void test_inline_format_identity() {
    require(parse_code_format("INLINE") == CodeFormat::inline_headers,
            "INLINE format token was not recognized");
    require(crypt_for_format(CodeFormat::inline_headers) == Crypt::raw,
            "INLINE should remain a RAW text-layer format");
    require(device_for_format(CodeFormat::inline_headers) == Device::standard,
            "INLINE format should not force one input device");
}

void test_inline_mixed_crypt_to_raw() {
    std::vector<std::uint32_t> ar1_words{0x20123456U, 0xDEADBEEFU};
    devices::action_replay::encrypt_ar1(ar1_words);

    char ar1_line[32]{};
    std::snprintf(ar1_line, sizeof(ar1_line), "%08X %08X",
                  ar1_words[0], ar1_words[1]);

    const std::string text =
        std::string("AR1 Code, CRYPT_AR1\n") + ar1_line + "\n" +
        "ARMAX Master, CRYPT_ARMAX\n"
        "VNVH-7KMA-DX61K\n"
        "A4UT-3BKQ-TYYV5\n"
        "RAW Code, CRYPT_RAW\n"
        "20111110 00000063\n";

    convert::Request request;
    request.input_format = CodeFormat::inline_headers;
    request.output_format = CodeFormat::standard_raw;
    request.transpose_mode = translate::TransposeMode::original;

    const convert::Result result = convert::convert_text(text, request);
    require(result.output_text.find("AR1 Code, CRYPT_RAW\n20123456 DEADBEEF") != std::string::npos,
            "INLINE did not decrypt the AR1-tagged block");
    require(result.output_text.find("ARMAX Master, CRYPT_RAW\nF0144134 00144137") != std::string::npos,
            "INLINE did not decode/decrypt the ARMAX-tagged block");
    require(result.output_text.find("RAW Code, CRYPT_RAW\n20111110 00000063") != std::string::npos,
            "INLINE did not preserve the RAW-tagged block");
}

void test_inline_ar2_context_across_blocks() {
    const std::string input =
        "Master, CRYPT_AR2\n"
        "0E3C7DF2 1853E59E\n"
        "EE8AA78A BCBDF29A\n"
        "Later Code, CRYPT_AR2\n"
        "CEB1E0D2 BCA99C84\n";

    convert::Request request;
    request.input_format = CodeFormat::inline_headers;
    request.output_format = CodeFormat::ar12_raw;

    const convert::Result result = convert::convert_text(input, request);
    require(result.output_text.find("F0145714 00145717") != std::string::npos,
            "INLINE AR2 master did not decrypt");
    require(result.output_text.find("104D16DC 00000101") != std::string::npos,
            "INLINE AR2 key context did not continue into the next block");
}

void test_inline_pnach_strips_tags() {
    const std::string input =
        "Group\n"
        "Code by Tester, CRYPT_RAW\n"
        "20123456 DEADBEEF\n";

    convert::Request request;
    request.input_format = CodeFormat::inline_headers;
    request.output_format = CodeFormat::pnach_raw;

    const convert::Result result = convert::convert_text(input, request);
    require(result.output_text.find("CRYPT_RAW") == std::string::npos,
            "PNACH output retained an INLINE crypt tag");
    require(result.output_text.find("[Group\\Code]") != std::string::npos,
            "INLINE PNACH output lost the organizer path");
    require(result.output_text.find("author=Tester") != std::string::npos,
            "INLINE PNACH output lost author metadata");
}

void test_inline_case_insensitive_tag_update() {
    const std::string input =
        "Mixed Case, crypt_raw\n"
        "20123456 DEADBEEF\n";

    convert::Request request;
    request.input_format = CodeFormat::inline_headers;
    request.output_format = CodeFormat::codebreaker_raw;

    const convert::Result result = convert::convert_text(input, request);
    require(result.output_text.find("Mixed Case, CRYPT_CRAW") != std::string::npos,
            "INLINE output did not update a mixed-case crypt tag");
}

void test_inline_full_armax_collection_matches_fixed_armax_input() {
    const std::string source = crypto_vectors::read_text(
        vector_root / "ARMAX[ENCRYPTED]-.hack__G.U. Vol 1_ Rebirth.txt");

    convert::Request fixed_request;
    fixed_request.input_format = CodeFormat::action_replay_max;
    fixed_request.output_format = CodeFormat::pnach_raw;
    fixed_request.transpose_mode = translate::TransposeMode::original;

    convert::Request inline_request = fixed_request;
    inline_request.input_format = CodeFormat::inline_headers;

    const convert::Result fixed = convert::convert_text(source, fixed_request);
    const convert::Result inlined = convert::convert_text(source, inline_request);
    require(inlined.output_text == fixed.output_text,
            "INLINE ARMAX collection output differs from fixed ARMAX input mode");
}

void test_inline_is_input_only() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::inline_headers;

    bool rejected = false;
    try {
        (void)convert::convert_text("Code\n20123456 DEADBEEF\n", request);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "INLINE was incorrectly accepted as an output format");
}

} // namespace

void run_inline_mode_tests() {
    test_inline_format_identity();
    test_inline_mixed_crypt_to_raw();
    test_inline_ar2_context_across_blocks();
    test_inline_pnach_strips_tags();
    test_inline_case_insensitive_tag_update();
    test_inline_full_armax_collection_matches_fixed_armax_input();
    test_inline_is_input_only();
}

} // namespace omni::tests
