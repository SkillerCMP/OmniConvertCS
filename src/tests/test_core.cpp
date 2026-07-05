#include "tests/test_core.hpp"

#include "formats/text/text_cheat_parser.hpp"
#include "formats/text/text_cheat_writer.hpp"
#include "core/cheat.hpp"
#include "core/crypt.hpp"
#include "core/inline_crypt.hpp"
#include "tests/test_support.hpp"
#include "util/bit_fields.hpp"
#include "util/newlines.hpp"

#include <cstdint>
#include <string>

namespace omni::tests {
namespace {

void test_cheat_container() {
    Cheat cheat;
    cheat.append_pair(0x20123456U, 0xDEADBEEFU);
    cheat.prepend_word(0x11111111U);
    require(cheat.word_count() == 3U, "cheat append/prepend failed");
    cheat.remove_words(1U, 2U);
    require(cheat.word_count() == 1U && cheat.words[0] == 0x11111111U, "cheat remove failed");
}

void test_text_round_trip() {
    const std::string input =
        "Max Money, CRYPT_CRAW\n"
        "20123456 DEADBEEF\n"
        "00123457 0000????\n"
        "# ignored comment\n";

    const auto blocks = formats::text::parse_text(input);
    require(blocks.size() == 1U, "block parse count failed");
    require(blocks[0].cheat.word_count() == 4U, "code parse count failed");
    require(blocks[0].wildcards.size() == 1U, "wildcard parse failed");

    const std::string output = formats::text::write_text(blocks, Crypt::gameshark_raw);
    require(output.find("CRYPT_GRAW") != std::string::npos, "crypt tag update failed");
    require(output.find("0000????") != std::string::npos, "wildcard output failed");
}


void test_newline_normalization() {
    require(newlines::to_lf("A\r\nB\nC\rD") == "A\nB\nC\nD",
            "mixed line endings did not normalize to LF");
    require(newlines::to_crlf("A\nB\n") == "A\r\nB\r\n",
            "LF to CRLF conversion failed");
    require(newlines::to_crlf("A\r\nB\r\n") == "A\r\nB\r\n",
            "existing CRLF was changed incorrectly");
    require(newlines::to_crlf("A\rB") == "A\r\nB",
            "standalone CR conversion failed");
    require(newlines::to_crlf("A\r\nB\nC\rD") == "A\r\nB\r\nC\r\nD",
            "mixed line endings did not normalize to CRLF");
    require(newlines::to_crlf(std::wstring_view(L"A\nB\rC")) == L"A\r\nB\r\nC",
            "wide clipboard newline conversion failed");
    require(newlines::to_crlf("").empty(),
            "empty newline conversion failed");
}

void test_old_mac_text_input() {
    const std::string input =
        "Max Money, CRYPT_CRAW\r"
        "20123456 DEADBEEF\r"
        "00123457 00000063\r";
    const auto blocks = formats::text::parse_text(input);
    require(blocks.size() == 1U && blocks[0].cheat.word_count() == 4U,
            "standalone CR text input collapsed into one line");
}

void test_bit_fields() {
    constexpr std::uint32_t word = 0xC412AE7CU;
    require(bits::extract(word, 30U, 2U) == 3U, "ARMAX M extraction failed");
    require(bits::extract(word, 27U, 3U) == 0U, "ARMAX O extraction failed");
    require(bits::extract(word, 25U, 2U) == 2U, "ARMAX W extraction failed");
    require((word & 0x01FFFFFFU) == 0x0012AE7CU, "ARMAX address extraction failed");
}

void test_crypt_mapping() {
    const auto crypt = parse_crypt("CRYPT_CB7_COMMON");
    require(crypt && *crypt == Crypt::codebreaker7_common, "crypt parse failed");
    require(device_for_crypt(Crypt::gameshark_raw) == Device::gameshark3,
            "crypt device mapping failed");
    require(crypt_from_label("Name, CRYPT_ARMAX") == Crypt::armax,
            "inline crypt parse failed");
}

} // namespace

void run_core_tests() {
    test_cheat_container();
    test_text_round_trip();
    test_newline_normalization();
    test_old_mac_text_input();
    test_bit_fields();
    test_crypt_mapping();
}

} // namespace omni::tests
