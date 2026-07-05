#include "tests/test_cb_gs_translation.hpp"

#include "core/cheat.hpp"
#include "tests/test_support.hpp"
#include "translate/translation_error.hpp"
#include "translate/translator.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

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


Cheat make_cheat(std::initializer_list<std::uint32_t> words) {
    Cheat cheat;
    cheat.words.reserve(words.size());
    for (const std::uint32_t word : words) {
        cheat.words.push_back(word);
    }
    return cheat;
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


void test_shared_direct_and_multiline_types() {
    Cheat direct = make_cheat({
        0x00100000U, 0x0000007FU,
        0x10100002U, 0x0000BEEFU,
        0x20100004U, 0x12345678U,
    });
    const std::initializer_list<std::uint32_t> direct_expected{
        0x00100000U, 0x0000007FU,
        0x10100002U, 0x0000BEEFU,
        0x20100004U, 0x12345678U,
    };
    translate::translate_cheat(direct, Device::codebreaker,
                              Device::gameshark3);
    require_words(direct, direct_expected,
                  "CB/GS direct writes should retain their public layout");

    Cheat serial = make_cheat({
        0x40100000U, 0x00030002U,
        0x00000001U, 0x00000002U,
    });
    translate::translate_cheat(serial, Device::gameshark3,
                              Device::codebreaker);
    require_words(serial, {
        0x40100000U, 0x00030002U,
        0x00000001U, 0x00000002U,
    }, "CB/GS type 4 serial record should retain both pairs");

    Cheat copy = make_cheat({
        0x50100000U, 0x00000020U,
        0x00200000U, 0x00000000U,
    });
    translate::translate_cheat(copy, Device::codebreaker,
                              Device::gameshark3);
    require_words(copy, {
        0x50100000U, 0x00000020U,
        0x00200000U, 0x00000000U,
    }, "CB/GS type 5 copy record should retain both pairs");
}

void test_pointer_translation() {
    Cheat cb_half = make_cheat({
        0x61123456U, 0x0000BEEFU,
        0x00010001U, 0x00000020U,
    });
    translate::translate_cheat(cb_half, Device::codebreaker,
                              Device::gameshark3);
    require_words(cb_half, {
        0x61123456U, 0x01000000U,
        0x00000020U, 0x0000BEEFU,
    }, "CB 16-bit pointer did not convert to GS 61 format");

    translate::translate_cheat(cb_half, Device::gameshark3,
                              Device::codebreaker);
    require_words(cb_half, {
        0x61123456U, 0x0000BEEFU,
        0x00010001U, 0x00000020U,
    }, "GS 61 pointer did not round-trip to CodeBreaker");

    Cheat gs_alias = make_cheat({
        0x60123456U, 0x00000000U,
        0x00000010U, 0x12345678U,
    });
    translate::translate_cheat(gs_alias, Device::gameshark3,
                              Device::codebreaker);
    require_words(gs_alias, {
        0x60123456U, 0x00005678U,
        0x00010001U, 0x00000010U,
    }, "GS 60 must map to the CodeBreaker 16-bit pointer width");

    Cheat cb_word = make_cheat({
        0x60123456U, 0x89ABCDEFU,
        0x00020001U, 0x00000024U,
    });
    translate::translate_cheat(cb_word, Device::codebreaker,
                              Device::gameshark3);
    require_words(cb_word, {
        0x62123456U, 0x00000000U,
        0x00000024U, 0x89ABCDEFU,
    }, "CB 32-bit pointer did not convert to GS 62 format");

    translate::translate_cheat(cb_word, Device::gameshark3,
                              Device::codebreaker);
    require_words(cb_word, {
        0x60123456U, 0x89ABCDEFU,
        0x00020001U, 0x00000024U,
    }, "GS 62 pointer did not round-trip to CodeBreaker");

    Cheat unsupported_gs = make_cheat({
        0x63123456U, 0U,
        0x10U, 0x1234U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(unsupported_gs, Device::gameshark3,
                                      Device::codebreaker);
        },
        translate::ErrorCode::pointer_size,
        "GS pointer subtype 63 should be rejected");

    Cheat zero_offset_count = make_cheat({
        0x60123456U, 0x1234U,
        0x00010000U, 0x10U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(zero_offset_count,
                                      Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::excess_offsets,
        "CB pointer count zero should not be treated as one GS dereference");
}

void test_setup_types_8_9_a() {
    Cheat type8 = make_cheat({0x80123457U, 0x0002007FU});
    translate::translate_cheat(type8, Device::codebreaker,
                              Device::gameshark3);
    require_words(type8, {0x80123456U, 0x0002007FU},
                  "type 8 setup gate alignment was not normalized");

    Cheat type9 = make_cheat({0x902274D7U, 0x0C08D8E2U});
    translate::translate_cheat(type9, Device::gameshark3,
                              Device::codebreaker);
    require_words(type9, {0x902274D4U, 0x0C08D8E2U},
                  "type 9 hook address was not aligned consistently");

    Cheat cb_once = make_cheat({0xA0123456U, 0x12345678U});
    translate::translate_cheat(cb_once, Device::codebreaker,
                              Device::gameshark3);
    require_words(cb_once, {0xA0123456U, 0x12345678U},
                  "CB one-shot type A did not map to GS one-shot type A");

    Cheat cb_odd_once = make_cheat({0xA0123457U, 0x89ABCDEFU});
    translate::translate_cheat(cb_odd_once, Device::codebreaker,
                              Device::gameshark3);
    require_words(cb_odd_once, {0xA0123456U, 0x89ABCDEFU},
                  "CB type A parser alignment was not preserved");

    Cheat gs_retained = make_cheat({0xA0123457U, 0x12345678U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(gs_retained, Device::gameshark3,
                                      Device::codebreaker);
        },
        translate::ErrorCode::semantic_loss,
        "retained GS type A should not become one-shot CB type A");
}

void test_type_b_and_c() {
    Cheat cb_delay = make_cheat({0xBABCDEF0U, 0x0000003CU});
    translate::translate_cheat(cb_delay, Device::codebreaker,
                              Device::gameshark3);
    require_words(cb_delay, {0xB0000000U, 0x0000003CU},
                  "CB B0 delay did not canonicalize for GameShark");

    Cheat gs_delay = make_cheat({0xBFFFFFF7U, 0x00000012U});
    translate::translate_cheat(gs_delay, Device::gameshark3,
                              Device::codebreaker);
    require_words(gs_delay, {0xB0000000U, 0x00000012U},
                  "GS B fields should be ignored when converting to CB B0");

    constexpr std::array<std::uint32_t, 3> internal_subtypes{{1U, 2U, 3U}};
    for (const std::uint32_t subtype : internal_subtypes) {
        Cheat control = make_cheat({0xB0000000U | subtype, 0U});
        require_translation_error(
            [&]() {
                translate::translate_cheat(control, Device::codebreaker,
                                          Device::gameshark3);
            },
            translate::ErrorCode::unsupported_control,
            "CB internal B subtype should not become a GS delay");
    }

    Cheat c_gate = make_cheat({0xC0100000U, 0x12345678U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(c_gate, Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::semantic_loss,
        "CB and GS type C lifetime mismatch should be rejected");
}

void test_single_conditions() {
    Cheat less_equal = make_cheat({0xD0100000U, 0x00200010U});
    translate::translate_cheat(less_equal, Device::codebreaker,
                              Device::gameshark3);
    require_words(less_equal, {0xD0100000U, 0x00200010U},
                  "CB unsigned <= should map directly to GS condition 2");

    Cheat less_than = make_cheat({0xD0100000U, 0x00300010U});
    translate::Report report;
    translate::translate_cheat(less_than, Device::codebreaker,
                              Device::gameshark3, &report);
    require_words(less_than, {0xD0100000U, 0x0020000FU},
                  "CB unsigned < should become GS unsigned <= value-1");
    require(!report.warnings.empty(),
            "exact less-than normalization should be reported");

    Cheat less_than_zero = make_cheat({0xD0100000U, 0x00300000U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(less_than_zero, Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::comparison,
        "CB unsigned < 0 should be rejected as always false");

    Cheat gs_always_true = make_cheat({
        0xD0100000U, 0x00300000U,
        0x20100004U, 0x12345678U,
    });
    translate::translate_cheat(gs_always_true, Device::gameshark3,
                              Device::codebreaker);
    require_words(gs_always_true, {0x20100004U, 0x12345678U},
                  "GS unsigned >= 0 condition should be removed");

    Cheat gs_greater_equal = make_cheat({0xD0100000U, 0x00300001U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(gs_greater_equal,
                                      Device::gameshark3,
                                      Device::codebreaker);
        },
        translate::ErrorCode::comparison,
        "GS unsigned >= should not be mislabeled as CB condition 3");

    Cheat cb_mask_test = make_cheat({0xD0100000U, 0x00500001U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(cb_mask_test, Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::comparison,
        "CB bit-mask D condition should be rejected for GS");

    Cheat cb_byte_test = make_cheat({0xD0100000U, 0x0001007FU});
    require_translation_error(
        [&]() {
            translate::translate_cheat(cb_byte_test, Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::test_size,
        "CB 8-bit D condition should be rejected for GS");
}

void test_multi_conditions() {
    Cheat cb_less = make_cheat({0xE0030010U, 0x30123456U});
    translate::translate_cheat(cb_less, Device::codebreaker,
                              Device::gameshark3);
    require_words(cb_less, {0xE003000FU, 0x20123456U},
                  "CB E unsigned < did not become GS E unsigned <= value-1");

    Cheat gs_always_true = make_cheat({
        0xE0010000U, 0x30123456U,
        0x20100000U, 0xABCDEF01U,
    });
    translate::translate_cheat(gs_always_true, Device::gameshark3,
                              Device::codebreaker);
    require_words(gs_always_true, {0x20100000U, 0xABCDEF01U},
                  "GS E unsigned >= 0 should be removed");

    Cheat large_count = make_cheat({0xE123BEEFU, 0x00123456U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(large_count, Device::gameshark3,
                                      Device::codebreaker);
        },
        translate::ErrorCode::count_overflow,
        "GS 12-bit E count should not overflow CB 8-bit count");
}

void test_type_f_modes() {
    constexpr std::array<std::uint32_t, 3> mode_parameters{{
        0x00001000U, 0x00001001U, 0x00001002U,
    }};
    for (const std::uint32_t parameter : mode_parameters) {
        Cheat mode = make_cheat({0xF012AE7CU, parameter});
        translate::translate_cheat(mode, Device::codebreaker,
                                  Device::gameshark3);
        require_words(mode, {0xF012AE7CU, parameter},
                      "CB F mode 0-2 should retain its parameter encoding for GS");
        translate::translate_cheat(mode, Device::gameshark3,
                                  Device::codebreaker);
        require_words(mode, {0xF012AE7CU, parameter},
                      "GS F mode 0-2 should round-trip to CB");
    }

    Cheat mode3 = make_cheat({0xF01EB2A0U, 0x001EE10FU});
    translate::translate_cheat(mode3, Device::codebreaker,
                              Device::gameshark3);
    require_words(mode3, {0xF01EB2A0U, 0x001EE10FU},
                  "CB F mode 3 should translate directly to GS");

    translate::translate_cheat(mode3, Device::gameshark3,
                              Device::codebreaker);
    require_words(mode3, {0xF01EB2A0U, 0x001EE10FU},
                  "GS F mode 3 should round-trip to CB");

    Cheat non_aligned = make_cheat({0xF01EB2A1U, 0x00000001U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(non_aligned, Device::gameshark3,
                                      Device::codebreaker);
        },
        translate::ErrorCode::unsupported_hook,
        "GS F low address bits should not be discarded by CB translation");

    Cheat cb_ignored = make_cheat({0xFFFFFFFFU, 0x12345678U});
    translate::Report report;
    translate::translate_cheat(cb_ignored, Device::codebreaker,
                              Device::gameshark3, &report);
    require(cb_ignored.words.empty(),
            "CodeBreaker FFFFFFFF no-op should be removed for GameShark");
    require(!report.warnings.empty(),
            "removing the CodeBreaker FFFFFFFF no-op should be reported");

    Cheat gs_real_f = make_cheat({0xFFFFFFFFU, 0x12345678U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(gs_real_f, Device::gameshark3,
                                      Device::codebreaker);
        },
        translate::ErrorCode::unsupported_hook,
        "GameShark FFFFFFFF setup must not become a CodeBreaker no-op");
}

void test_remaining_shared_types() {
    Cheat increment = make_cheat({
        0x30400000U, 0x00100000U,
        0x00000005U, 0x00000000U,
    });
    translate::translate_cheat(increment, Device::codebreaker,
                              Device::gameshark3);
    require_words(increment, {
        0x30400000U, 0x00100000U,
        0x00000005U, 0x00000000U,
    }, "32-bit increment continuation was not preserved");

    Cheat invalid_increment = make_cheat({0x30600000U, 0x00100000U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(invalid_increment,
                                      Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::invalid_code,
        "increment subtype 6 should be rejected");

    Cheat bitwise = make_cheat({0x70100000U, 0x00000001U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(bitwise, Device::codebreaker,
                                      Device::gameshark3);
        },
        translate::ErrorCode::bitwise,
        "CB type 7 should not become GS verifier metadata");
}

} // namespace

void run_cb_gs_translation_tests() {
    test_shared_direct_and_multiline_types();
    test_pointer_translation();
    test_setup_types_8_9_a();
    test_type_b_and_c();
    test_single_conditions();
    test_multi_conditions();
    test_type_f_modes();
    test_remaining_shared_types();
}

} // namespace omni::tests
