#include "tests/test_translation.hpp"

#include "convert/conversion_service.hpp"
#include "core/cheat.hpp"
#include "devices/armax/armax_crypto.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/test_support.hpp"
#include "translate/translation_error.hpp"
#include "translate/translator.hpp"

#include <cstdint>
#include <functional>
#include <string>

namespace omni::tests {
namespace {

void require_words(const Cheat& cheat, std::initializer_list<std::uint32_t> expected,
                   std::string_view message) {
    require(cheat.words.size() == expected.size(), message);
    std::size_t index = 0U;
    for (const std::uint32_t value : expected) {
        require(cheat.words[index] == value, message);
        ++index;
    }
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

void test_direct_write_round_trip() {
    Cheat cheat;
    cheat.words = {
        0x00100000U, 0x00000012U,
        0x10100002U, 0x00003456U,
        0x20100004U, 0x89ABCDEFU,
    };
    translate::translate_cheat(cheat, Device::standard, Device::armax);
    require_words(cheat, {
        0x00100000U, 0x00000012U,
        0x02100002U, 0x00003456U,
        0x04100004U, 0x89ABCDEFU,
    }, "standard to ARMAX direct writes failed");

    translate::translate_cheat(cheat, Device::armax, Device::codebreaker);
    require_words(cheat, {
        0x00100000U, 0x00000012U,
        0x10100002U, 0x00003456U,
        0x20100004U, 0x89ABCDEFU,
    }, "ARMAX direct write round trip failed");
}

void test_armax_fill_count_is_plus_one() {
    Cheat cheat;
    cheat.words = {0x00001000U, 0x0000027FU}; // count-minus-one = 2 => 3 bytes
    translate::translate_cheat(cheat, Device::armax, Device::codebreaker);
    require_words(cheat, {
        0x10001000U, 0x00007F7FU,
        0x00001002U, 0x0000007FU,
    }, "ARMAX direct fill count was not decoded as count plus one");
}

void test_armax_serial_fields() {
    Cheat cheat;
    cheat.words = {
        0x00000000U, 0x84001000U,
        0x12345678U, 0x01020008U,
    };
    translate::translate_cheat(cheat, Device::armax, Device::codebreaker);
    require_words(cheat, {
        0x40001000U, 0x00030002U,
        0x12345678U, 0x00000001U,
    }, "ARMAX serial count/stride/increment decoding failed");

    translate::translate_cheat(cheat, Device::codebreaker, Device::armax);
    require_words(cheat, {
        0x00000000U, 0x84001000U,
        0x12345678U, 0x01020008U,
    }, "standard serial to ARMAX packing failed");
}

void test_word_increment_continuation() {
    Cheat cheat;
    cheat.words = {
        0x30400000U, 0x00100000U,
        0x00000005U, 0x00000000U,
    };
    translate::translate_cheat(cheat, Device::codebreaker, Device::armax);
    require_words(cheat, {0x84100000U, 0x00000005U},
                  "32-bit increment did not consume its continuation line");

    translate::translate_cheat(cheat, Device::armax, Device::codebreaker);
    require_words(cheat, {
        0x30400000U, 0x00100000U,
        0x00000005U, 0x00000000U,
    }, "32-bit increment continuation was not emitted");
}

void test_cb_gs_layout_rules() {
    Cheat type8;
    type8.words = {0x80100000U, 0x0002007FU};
    translate::translate_cheat(type8, Device::codebreaker, Device::gameshark3);
    require_words(type8, {0x80100000U, 0x0002007FU},
                  "CB type 8 should be accepted by GS translation");

    Cheat byte_e;
    byte_e.words = {0xE103007FU, 0x00100000U};
    require_translation_error(
        [&]() { translate::translate_cheat(byte_e, Device::codebreaker,
                                          Device::gameshark3); },
        translate::ErrorCode::test_size,
        "CB 8-bit E should not be flattened into GS fixed-16-bit E");

    Cheat large_gs_e;
    large_gs_e.words = {0xE123BEEFU, 0x00100000U};
    require_translation_error(
        [&]() { translate::translate_cheat(large_gs_e, Device::gameshark3,
                                          Device::codebreaker); },
        translate::ErrorCode::count_overflow,
        "GS 12-bit E count should not overflow CB 8-bit count");
}

void test_semantic_loss_is_rejected() {
    Cheat b_subtype;
    b_subtype.words = {0xB0000001U, 0U};
    require_translation_error(
        [&]() { translate::translate_cheat(b_subtype, Device::codebreaker,
                                          Device::gameshark3); },
        translate::ErrorCode::unsupported_control,
        "CB B subtype 1 should not become a GS delay");

    Cheat c_gate;
    c_gate.words = {0xC0100000U, 0x12345678U};
    require_translation_error(
        [&]() { translate::translate_cheat(c_gate, Device::codebreaker,
                                          Device::gameshark3); },
        translate::ErrorCode::semantic_loss,
        "CB/GS C lifetime mismatch should be rejected");

    Cheat once;
    once.words = {0xA0100000U, 0x12345678U};
    require_translation_error(
        [&]() { translate::translate_cheat(once, Device::codebreaker,
                                          Device::armax); },
        translate::ErrorCode::semantic_loss,
        "one-time A write should not become persistent ARMAX write");

    Cheat cb_hook;
    cb_hook.words = {0xF01EB2A0U, 0x001EE10FU};
    require_translation_error(
        [&]() { translate::translate_cheat(cb_hook, Device::codebreaker,
                                          Device::armax); },
        translate::ErrorCode::unsupported_hook,
        "CB F mode/secondary hook should not be discarded");
}

void test_armax_condition_safety() {
    Cheat signed_compare;
    signed_compare.words = {
        0x18100000U, 0x00000001U,
        0x00100001U, 0x00000002U,
    };
    require_translation_error(
        [&]() { translate::translate_cheat(signed_compare, Device::armax,
                                          Device::codebreaker); },
        translate::ErrorCode::comparison,
        "signed ARMAX comparison should not become unsigned");

    Cheat retry_compare;
    retry_compare.words = {
        0xC8100000U, 0x00000001U,
        0x00100001U, 0x00000002U,
    };
    require_translation_error(
        [&]() { translate::translate_cheat(retry_compare, Device::armax,
                                          Device::codebreaker); },
        translate::ErrorCode::semantic_loss,
        "ARMAX mode-3 retry condition should not be flattened");

    Cheat word_compare;
    word_compare.words = {
        0x0C100000U, 0x12345678U,
        0x00100001U, 0x00000002U,
    };
    translate::translate_cheat(word_compare, Device::armax,
                              Device::codebreaker);
    require_words(word_compare, {
        0xC0100000U, 0x12345678U,
        0x00100001U, 0x00000002U,
    }, "ARMAX 32-bit equal-at-end should map to CodeBreaker type C");
}

void test_conditional_hook_translation() {
    Cheat cheat;
    cheat.words = {
        0x0C100000U, 0x12345678U,
        0xC4100000U, 0x0003FF00U,
    };
    translate::translate_cheat(cheat, Device::armax, Device::codebreaker);
    require_words(cheat, {0x90100000U, 0x12345678U},
                  "ARMAX conditional hook did not become type 9");
}


void test_ar2_codebreaker_type_f_passthrough() {
    Cheat ar2_hook;
    ar2_hook.words = {0xF0145714U, 0x00145717U};
    translate::translate_cheat(ar2_hook, Device::ar2, Device::codebreaker);
    require_words(ar2_hook, {0xF0145714U, 0x00145717U},
                  "AR2 type-F master hook should pass through to CodeBreaker RAW");

    translate::translate_cheat(ar2_hook, Device::codebreaker, Device::ar2);
    require_words(ar2_hook, {0xF0145714U, 0x00145717U},
                  "CodeBreaker type-F master hook should round-trip to AR2 RAW");

    Cheat ar1_hook;
    ar1_hook.words = {0xF0145714U, 0x00145717U};
    translate::translate_cheat(ar1_hook, Device::ar1, Device::codebreaker);
    require_words(ar1_hook, {0xF0145714U, 0x00145717U},
                  "AR1 type-F master hook should pass through to CodeBreaker RAW");

    Cheat armax_hook;
    armax_hook.words = {0xF0145714U, 0x00145717U};
    translate::translate_cheat(armax_hook, Device::ar2, Device::armax);
    require_words(armax_hook, {0xC4145714U, 0x0003FF00U},
                  "AR2 canonical type-F hook should map to ARMAX C4 standard hook");

    translate::translate_cheat(armax_hook, Device::armax, Device::ar2);
    require_words(armax_hook, {0xF0145714U, 0x00145717U},
                  "ARMAX C4 standard hook should round-trip to canonical AR2 type F");

    convert::Request request;
    request.input_format = CodeFormat::action_replay2;
    request.output_format = CodeFormat::codebreaker_raw;

    const convert::Result result = convert::convert_text(
        "(M) Must Be On by Codejunkies , CRYPT_AR2\n"
        "0E3C7DF2 1853E59E\n"
        "EE8AA78A BCBDF29A\n",
        request);

    require(result.output_text.find("F0145714 00145717") != std::string::npos,
            "AR2 encrypted master code did not convert to CodeBreaker RAW type F");

    request.output_format = CodeFormat::armax_raw;
    const convert::Result armax_result = convert::convert_text(
        "(M) Must Be On by Codejunkies , CRYPT_AR2\n"
        "0E3C7DF2 1853E59E\n"
        "EE8AA78A BCBDF29A\n",
        request);

    require(armax_result.output_text.find("C4145714 0003FF00") !=
                std::string::npos,
            "AR2 encrypted master code did not convert to ARMAX RAW C4 hook");

    convert::Request reverse_request;
    reverse_request.input_format = CodeFormat::armax_raw;
    reverse_request.output_format = CodeFormat::ar12_raw;
    const convert::Result reverse_result = convert::convert_text(
        armax_result.output_text, reverse_request);
    require(reverse_result.output_text.find("F0145714 00145717") !=
                std::string::npos,
            "ARMAX RAW C4 standard hook did not round-trip to AR RAW type F");
}

void test_generated_armax_verifier() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::armax_raw;
    request.armax_game_id = 0x123U;
    request.armax_region = 2U;
    request.armax_verifier_nonce = 0x4567U;

    const convert::Result result = convert::convert_text(
        "Test\n00100000 0000007F\n", request);
    const auto blocks = formats::text::parse_text(result.output_text, Crypt::max_raw);
    require(blocks.size() == 1U, "generated ARMAX RAW parse failed");
    require(blocks[0].cheat.words.size() == 4U,
            "generated ARMAX verifier was not prepended");
    require(((blocks[0].cheat.words[0] >> 15U) & 0x1FFFU) == 0x123U,
            "generated ARMAX game ID is wrong");
    require(((blocks[0].cheat.words[1] >> 24U) & 3U) == 2U,
            "generated ARMAX region is wrong");
    require(devices::armax::read_verifier_lines(blocks[0].cheat.words) == 1U,
            "generated ARMAX verifier is not structurally valid");
}

} // namespace

void run_translation_tests() {
    test_direct_write_round_trip();
    test_armax_fill_count_is_plus_one();
    test_armax_serial_fields();
    test_word_increment_continuation();
    test_cb_gs_layout_rules();
    test_semantic_loss_is_rejected();
    test_armax_condition_safety();
    test_conditional_hook_translation();
    test_ar2_codebreaker_type_f_passthrough();
    test_generated_armax_verifier();
}

} // namespace omni::tests
