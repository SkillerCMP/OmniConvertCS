#include "tests/test_armax_translation.hpp"

#include "core/cheat.hpp"
#include "tests/test_support.hpp"
#include "translate/translation_error.hpp"
#include "translate/translator.hpp"

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
    for (const std::uint32_t word : expected) {
        require(cheat.words[index] == word, message);
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

void test_standard_comparison_expansion() {
    Cheat cb_less_equal = make_cheat({
        0xD0100000U, 0x00200010U,
        0x20100004U, 0x12345678U,
    });
    translate::Report cb_report;
    translate::translate_cheat(cb_less_equal, Device::codebreaker,
                              Device::armax, &cb_report);
    require_words(cb_less_equal, {
        0x2A100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
        0x0A100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
    }, "CB <= did not expand into ARMAX unsigned-less OR equal");
    require(!cb_report.warnings.empty(),
            "CB <= ARMAX expansion should be reported");

    Cheat gs_greater_equal = make_cheat({
        0xD0100000U, 0x00300010U,
        0x20100004U, 0x12345678U,
    });
    translate::translate_cheat(gs_greater_equal, Device::gameshark3,
                              Device::armax);
    require_words(gs_greater_equal, {
        0x32100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
        0x0A100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
    }, "GS >= did not expand into ARMAX unsigned-greater OR equal");
}

void test_standard_constant_comparisons() {
    Cheat always_true = make_cheat({
        0xD0100000U, 0x0020FFFFU,
        0x20100004U, 0x12345678U,
    });
    translate::Report true_report;
    translate::translate_cheat(always_true, Device::codebreaker,
                              Device::armax, &true_report);
    require_words(always_true, {0x04100004U, 0x12345678U},
                  "CB <= FFFF should become an unconditional body");
    require(!true_report.warnings.empty(),
            "always-true comparison removal should be reported");

    Cheat always_false = make_cheat({
        0xD0100000U, 0x00300000U,
        0x20100004U, 0x12345678U,
    });
    translate::Report false_report;
    translate::translate_cheat(always_false, Device::codebreaker,
                              Device::armax, &false_report);
    require(always_false.words.empty(),
            "CB unsigned < 0 and its body should be removed");
    require(!false_report.warnings.empty(),
            "always-false comparison removal should be reported");
}

void test_armax_unsigned_comparisons() {
    Cheat less_to_cb = make_cheat({
        0x2A100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
    });
    translate::translate_cheat(less_to_cb, Device::armax,
                              Device::codebreaker);
    require_words(less_to_cb, {
        0xD0100000U, 0x00300010U,
        0x20100004U, 0x12345678U,
    }, "ARMAX unsigned less-than did not map to CB condition 3");

    Cheat less_to_gs = make_cheat({
        0x2A100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
    });
    translate::translate_cheat(less_to_gs, Device::armax,
                              Device::gameshark3);
    require_words(less_to_gs, {
        0xD0100000U, 0x0020000FU,
        0x20100004U, 0x12345678U,
    }, "ARMAX unsigned less-than did not normalize to GS <= value-1");

    Cheat greater_to_gs = make_cheat({
        0x32100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
    });
    translate::translate_cheat(greater_to_gs, Device::armax,
                              Device::gameshark3);
    require_words(greater_to_gs, {
        0xD0100000U, 0x00300011U,
        0x20100004U, 0x12345678U,
    }, "ARMAX unsigned greater-than did not normalize to GS >= value+1");

    Cheat greater_to_cb = make_cheat({
        0x32100000U, 0x00000010U,
        0x04100004U, 0x12345678U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(greater_to_cb, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::comparison,
        "ARMAX unsigned greater-than should not be mislabeled for CB");

    Cheat and_to_cb = make_cheat({
        0x3A100000U, 0x00000080U,
        0x04100004U, 0x12345678U,
    });
    translate::translate_cheat(and_to_cb, Device::armax,
                              Device::codebreaker);
    require_words(and_to_cb, {
        0xD0100000U, 0x00500080U,
        0x20100004U, 0x12345678U,
    }, "ARMAX AND did not map to CB condition 5");
}

void test_armax_constant_false_comparison() {
    Cheat less_zero = make_cheat({
        0x2A100000U, 0x00000000U,
        0x04100004U, 0x12345678U,
    });
    translate::Report report;
    translate::translate_cheat(less_zero, Device::armax,
                              Device::gameshark3, &report);
    require(less_zero.words.empty(),
            "ARMAX unsigned < 0 should remove its controlled body");
    require(!report.warnings.empty(),
            "ARMAX always-false comparison should be reported");
}

void test_word_equal_gate_mapping() {
    Cheat end_gate = make_cheat({
        0x0C100000U, 0x12345678U,
        0x04100004U, 0xABCDEF01U,
    });
    translate::translate_cheat(end_gate, Device::armax,
                              Device::codebreaker);
    require_words(end_gate, {
        0xC0100000U, 0x12345678U,
        0x20100004U, 0xABCDEF01U,
    }, "ARMAX 32-bit equal-at-end did not map to CB type C");

    Cheat block_gate = make_cheat({
        0x8C100000U, 0x12345678U,
        0x04100004U, 0xABCDEF01U,
        0x00000000U, 0x40000000U,
    });
    translate::translate_cheat(block_gate, Device::armax,
                              Device::codebreaker);
    require_words(block_gate, {
        0xC0100000U, 0x12345678U,
        0x20100004U, 0xABCDEF01U,
    }, "ARMAX mode-2 32-bit equal block did not map to CB type C");

    Cheat early_gate = make_cheat({
        0x0C100000U, 0x12345678U,
        0x04100004U, 0xABCDEF01U,
        0x04100008U, 0x10203040U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(early_gate, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::semantic_loss,
        "ARMAX 32-bit equal that ends early should not become CB type C");
}

void test_pointer_operation_mappings() {
    Cheat byte_pointer = make_cheat({0x40100000U, 0xFFFFFE7FU});
    translate::translate_cheat(byte_pointer, Device::armax,
                              Device::codebreaker);
    require_words(byte_pointer, {
        0x60100000U, 0x0000007FU,
        0x00000001U, 0xFFFFFFFEU,
    }, "ARMAX signed 8-bit pointer offset was decoded incorrectly");
    translate::translate_cheat(byte_pointer, Device::codebreaker,
                              Device::armax);
    require_words(byte_pointer, {0x40100000U, 0xFFFFFE7FU},
                  "8-bit pointer did not round-trip through CB");

    Cheat half_pointer = make_cheat({0x42100000U, 0xFFFEBEEFU});
    translate::translate_cheat(half_pointer, Device::armax,
                              Device::codebreaker);
    require_words(half_pointer, {
        0x60100000U, 0x0000BEEFU,
        0x00010001U, 0xFFFFFFFCU,
    }, "ARMAX signed 16-bit pointer offset was decoded incorrectly");

    Cheat word_pointer = make_cheat({0x44100000U, 0xDEADBEEFU});
    translate::translate_cheat(word_pointer, Device::armax,
                              Device::gameshark3);
    require_words(word_pointer, {
        0x62100000U, 0x00000000U,
        0x00000000U, 0xDEADBEEFU,
    }, "ARMAX 32-bit pointer did not map to GS subtype 62");

    Cheat cb_zero_depth = make_cheat({
        0x60100000U, 0x00001234U,
        0x00010000U, 0x00000010U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(cb_zero_depth,
                                      Device::codebreaker,
                                      Device::armax);
        },
        translate::ErrorCode::excess_offsets,
        "CB zero-depth pointer should not become an ARMAX dereference");

    Cheat gs_reserved = make_cheat({
        0x63100000U, 0x00000000U,
        0x00000000U, 0x00001234U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(gs_reserved,
                                      Device::gameshark3,
                                      Device::armax);
        },
        translate::ErrorCode::pointer_size,
        "GS pointer subtype 63 should be rejected");
}

void test_serial_operation_mappings() {
    Cheat signed_stride = make_cheat({
        0x00000000U, 0x80001004U,
        0x00000010U, 0xFF02FFFFU,
    });
    translate::Report report;
    translate::translate_cheat(signed_stride, Device::armax,
                              Device::codebreaker, &report);
    require_words(signed_stride, {
        0x00001004U, 0x00000010U,
        0x00001003U, 0x0000000FU,
        0x00001002U, 0x0000000EU,
    }, "ARMAX signed byte serial was not expanded correctly");
    require(!report.warnings.empty(),
            "serial expansion should be reported");

    Cheat out_of_range = make_cheat({
        0x00000000U, 0x80000000U,
        0x00000010U, 0x0001FFFFU,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(out_of_range, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::serial_size,
        "serial address underflow should be rejected");
}


void test_zero_address_byte_write_encoding() {
    Cheat byte_zero = make_cheat({0x00000000U, 0x0000007FU});
    translate::Report report;
    translate::translate_cheat(byte_zero, Device::codebreaker,
                              Device::armax, &report);
    require_words(byte_zero, {
        0x00000000U, 0x80000000U,
        0x0000007FU, 0x00000001U,
    }, "byte write at address zero must use an ARMAX serial descriptor");
    require(!report.warnings.empty(),
            "address-zero byte-write expansion should be reported");

    translate::translate_cheat(byte_zero, Device::armax,
                              Device::codebreaker);
    require_words(byte_zero, {0x00000000U, 0x0000007FU},
                  "address-zero byte write did not round-trip");
}

void test_large_armax_fill_chunking() {
    // 524,281 bytes require two maximum-size standard type-4 records plus
    // one trailing byte write. This verifies that the 16-bit standard count
    // field is never overflowed while expanding an ARMAX fill.
    Cheat fill = make_cheat({0x00001000U, 0x07FFF87FU});
    translate::Report report;
    translate::translate_cheat(fill, Device::armax,
                              Device::codebreaker, &report);
    require_words(fill, {
        0x40001000U, 0xFFFF0001U,
        0x7F7F7F7FU, 0x00000000U,
        0x40040FFCU, 0xFFFF0001U,
        0x7F7F7F7FU, 0x00000000U,
        0x00080FF8U, 0x0000007FU,
    }, "large ARMAX fill did not split at the standard 16-bit count limit");
    require(!report.warnings.empty(),
            "large fill expansion should be reported");
}

void test_standard_serial_splitting_and_expansion() {
    Cheat split = make_cheat({
        0x40100000U, 0x01010001U,
        0x00000020U, 0x00000001U,
    });
    translate::Report split_report;
    translate::translate_cheat(split, Device::codebreaker,
                              Device::armax, &split_report);
    require_words(split, {
        0x00000000U, 0x84100000U,
        0x00000020U, 0x01000004U,
        0x00000000U, 0x84100004U,
        0x00000021U, 0x01FF0004U,
    }, "257-item standard serial did not split into 1 plus 256 ARMAX items");
    require(!split_report.warnings.empty(),
            "ARMAX serial splitting should be reported");

    Cheat exploded = make_cheat({
        0x40100000U, 0x00020001U,
        0x00000010U, 0x00000100U,
    });
    translate::Report explode_report;
    translate::translate_cheat(exploded, Device::codebreaker,
                              Device::armax, &explode_report);
    require_words(exploded, {
        0x04100000U, 0x00000010U,
        0x04100004U, 0x00000110U,
    }, "out-of-range ARMAX serial increment did not expand exactly");
    require(!explode_report.warnings.empty(),
            "serial-to-direct expansion should be reported");
}

void test_word_serial_can_cross_address_field_after_start() {
    Cheat crossing = make_cheat({
        0x00000000U, 0x85FFFFFCU,
        0x12345678U, 0x00010004U,
    });
    translate::translate_cheat(crossing, Device::armax,
                              Device::codebreaker);
    require_words(crossing, {
        0x41FFFFFCU, 0x00020001U,
        0x12345678U, 0x00000000U,
    }, "an exact word-serial mapping should preserve runtime address progression");
}

void test_target_independent_false_predicates() {
    Cheat greater_max = make_cheat({
        0x32100000U, 0x0000FFFFU,
        0x04100004U, 0x12345678U,
    });
    translate::Report greater_report;
    translate::translate_cheat(greater_max, Device::armax,
                              Device::codebreaker, &greater_report);
    require(greater_max.words.empty(),
            "unsigned halfword > FFFF should be removed as always false");
    require(!greater_report.warnings.empty(),
            "always-false greater-than removal should be reported");

    Cheat and_zero = make_cheat({
        0x3A100000U, 0x00000000U,
        0x04100004U, 0x12345678U,
    });
    translate::Report and_report;
    translate::translate_cheat(and_zero, Device::armax,
                              Device::gameshark3, &and_report);
    require(and_zero.words.empty(),
            "ARMAX AND zero should be removed before target predicate lookup");
    require(!and_report.warnings.empty(),
            "always-false AND removal should be reported");
}

void test_condition_record_and_command_counts() {
    // ARMAX mode 1 skips two physical records. Together these two records
    // form one serial operation, so the standard target must use a single
    // D condition rather than an E condition with count 2.
    Cheat serial_body = make_cheat({
        0x4A100000U, 0x00000010U,
        0x00000000U, 0x84001000U,
        0x00000020U, 0x00010004U,
    });
    translate::translate_cheat(serial_body, Device::armax,
                              Device::codebreaker);
    require_words(serial_body, {
        0xD0100000U, 0x00000010U,
        0x40001000U, 0x00020001U,
        0x00000020U, 0x00000000U,
    }, "ARMAX skip-2 serial body should count as one standard command");

    // A mode-0 skip-one condition cannot safely control half of a two-record
    // serial operation. Reject it instead of changing the skip boundary.
    Cheat split_serial = make_cheat({
        0x0A100000U, 0x00000010U,
        0x00000000U, 0x84001000U,
        0x00000020U, 0x00010004U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(split_serial, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::invalid_code,
        "ARMAX skip-one must not cut through a two-record serial operation");
}

void test_zero_word_controls() {
    Cheat reserved_noop = make_cheat({0x00000000U, 0x20000000U});
    translate::Report report;
    translate::translate_cheat(reserved_noop, Device::armax,
                              Device::codebreaker, &report);
    require(reserved_noop.words.empty(),
            "ARMAX selector-1 no-op should be removed");
    require(!report.warnings.empty(),
            "removing selector-1 should be reported");

    Cheat no_effect_condition = make_cheat({
        0x1A100000U, 0x00000001U,
        0x00000000U, 0x20000000U,
    });
    translate::Report no_effect_report;
    translate::translate_cheat(no_effect_condition, Device::armax,
                              Device::codebreaker, &no_effect_report);
    require(no_effect_condition.words.empty(),
            "a condition controlling only an ARMAX no-op should be removed");
    require(!no_effect_report.warnings.empty(),
            "no-effect conditional removal should be reported");

    for (const std::uint32_t control : {
             0x00000000U, 0x40000000U, 0x60000000U}) {
        Cheat unsupported = make_cheat({0x00000000U, control});
        require_translation_error(
            [&]() {
                translate::translate_cheat(unsupported, Device::armax,
                                          Device::codebreaker);
            },
            translate::ErrorCode::unsupported_control,
            "standalone ARMAX state control should be rejected");
    }
}

void test_unrepresentable_armax_operations() {
    Cheat float_add = make_cheat({0x86100000U, 0x00000005U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(float_add, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::value_increment,
        "ARMAX float-add should not become integer type 3");

    Cheat special_region = make_cheat({0xC6100000U, 0x12345678U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(special_region, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::semantic_loss,
        "ARMAX C6 special-region write should not become a normal write");

    Cheat standalone_hook = make_cheat({0xC4100000U, 0x00030800U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(standalone_hook, Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::unsupported_hook,
        "ARMAX C4 template fields should not be discarded");

    Cheat nonstandard_conditional_hook = make_cheat({
        0x0C100000U, 0x12345678U,
        0xC4100000U, 0x00030800U,
    });
    require_translation_error(
        [&]() {
            translate::translate_cheat(nonstandard_conditional_hook,
                                      Device::armax,
                                      Device::codebreaker);
        },
        translate::ErrorCode::unsupported_hook,
        "conditional C4 with another template should be rejected");

    Cheat gs_f = make_cheat({0xF012AE7CU, 0x00001001U});
    require_translation_error(
        [&]() {
            translate::translate_cheat(gs_f, Device::gameshark3,
                                      Device::armax);
        },
        translate::ErrorCode::unsupported_hook,
        "GS type F should not be normalized to an arbitrary ARMAX hook template");
}

} // namespace

void run_armax_translation_tests() {
    test_standard_comparison_expansion();
    test_standard_constant_comparisons();
    test_armax_unsigned_comparisons();
    test_armax_constant_false_comparison();
    test_word_equal_gate_mapping();
    test_pointer_operation_mappings();
    test_serial_operation_mappings();
    test_zero_address_byte_write_encoding();
    test_large_armax_fill_chunking();
    test_standard_serial_splitting_and_expansion();
    test_word_serial_can_cross_address_field_after_start();
    test_target_independent_false_predicates();
    test_condition_record_and_command_counts();
    test_zero_word_controls();
    test_unrepresentable_armax_operations();
}

} // namespace omni::tests
