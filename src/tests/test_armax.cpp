#include "tests/test_armax.hpp"

#include "convert/conversion_service.hpp"
#include "devices/armax/armax_codec.hpp"
#include "devices/armax/armax_crypto.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/crypto/vector_file.hpp"
#include "tests/test_support.hpp"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef OMNI_TEST_VECTOR_DIR
#error OMNI_TEST_VECTOR_DIR must be defined for crypto vector tests
#endif

namespace omni::tests {
namespace {

const std::filesystem::path vector_root{OMNI_TEST_VECTOR_DIR};
constexpr const char* raw_file =
    "ARMAX[DECRYPTED-Armax(RAW)]-.hack__G.U. Vol 1_ Rebirth - Copy.txt";
constexpr const char* encrypted_file =
    "ARMAX[ENCRYPTED]-.hack__G.U. Vol 1_ Rebirth.txt";

std::vector<std::vector<std::uint32_t>> non_empty_block_words(
    const std::vector<formats::text::CheatBlock>& blocks) {
    std::vector<std::vector<std::uint32_t>> result;
    for (const auto& block : blocks) {
        if (!block.cheat.empty()) result.push_back(block.cheat.words);
    }
    return result;
}

std::vector<std::vector<std::uint32_t>> load_blocks(const char* file_name,
                                                     Crypt crypt) {
    return non_empty_block_words(formats::text::parse_text(
        crypto_vectors::read_text(vector_root / file_name), crypt));
}

std::size_t count_pairs(const std::vector<std::vector<std::uint32_t>>& blocks) {
    std::size_t result = 0U;
    for (const auto& words : blocks) result += words.size() / 2U;
    return result;
}

void test_alphabet_codec() {
    const auto lines = crypto_vectors::parse_armax_lines(
        crypto_vectors::read_text(vector_root / encrypted_file));
    require(lines.size() == 274U, "ARMAX encrypted vector count changed");

    for (const std::string& line : lines) {
        bool parity_valid = false;
        const auto decoded = devices::armax::decode_line(line, &parity_valid);
        require(decoded.has_value(), "ARMAX supplied line was not decoded");
        require(parity_valid, "ARMAX supplied line parity check failed");
        require(devices::armax::encode_line(*decoded) == line,
                "ARMAX text codec did not reproduce a supplied line");
    }
}

void test_supplied_vectors() {
    const auto raw_blocks = load_blocks(raw_file, Crypt::max_raw);
    const auto encrypted_blocks = load_blocks(encrypted_file, Crypt::armax);

    require(!raw_blocks.empty(), "ARMAX RAW vector did not contain code blocks");
    require(raw_blocks.size() == encrypted_blocks.size(),
            "ARMAX encrypted/decrypted cheat block counts differ");
    require(count_pairs(raw_blocks) == 274U,
            "ARMAX RAW supplied vector count changed");
    require(count_pairs(encrypted_blocks) == 274U,
            "ARMAX encrypted supplied vector count changed");

    for (std::size_t index = 0U; index < raw_blocks.size(); ++index) {
        auto generated = raw_blocks[index];
        devices::armax::encrypt_full(generated);
        require(generated == encrypted_blocks[index],
                "ARMAX encryption differs from supplied vectors");

        const auto metadata = devices::armax::decrypt_full(generated);
        require(generated == raw_blocks[index],
                "ARMAX decryption differs from supplied vectors");
        require(metadata.game_id == 0x09CBU,
                "ARMAX verifier game ID differs from supplied vector metadata");
        require(metadata.region <= 2U,
                "ARMAX verifier region is outside the supported range");
    }
}

void test_text_conversion_service() {
    const std::string raw_text = crypto_vectors::read_text(vector_root / raw_file);
    const std::string encrypted_text =
        crypto_vectors::read_text(vector_root / encrypted_file);

    const convert::Request encrypt_request{
        CodeFormat::armax_raw, CodeFormat::action_replay_max,
        devices::action_replay::ar1_seed,
        devices::gameshark::default_key,
        devices::armax::default_payload_key};
    convert::Request compatible_encrypt_request = encrypt_request;
    compatible_encrypt_request.armax_verifier_mode =
        convert::ArmaxVerifierMode::manual;
    const convert::Result encrypted_result =
        convert::convert_text(raw_text, compatible_encrypt_request);
    require(non_empty_block_words(formats::text::parse_text(
                encrypted_result.output_text, Crypt::armax)) ==
            non_empty_block_words(formats::text::parse_text(
                encrypted_text, Crypt::armax)),
            "ARMAX text encryption differs from supplied vectors");

    const convert::Request decrypt_request{
        CodeFormat::action_replay_max, CodeFormat::armax_raw,
        devices::action_replay::ar1_seed,
        devices::gameshark::default_key,
        devices::armax::default_payload_key};
    const convert::Result decrypted_result =
        convert::convert_text(encrypted_text, decrypt_request);
    require(non_empty_block_words(formats::text::parse_text(
                decrypted_result.output_text, Crypt::max_raw)) ==
            non_empty_block_words(formats::text::parse_text(
                raw_text, Crypt::max_raw)),
            "ARMAX text decryption differs from supplied vectors");
}

void test_reserved_label_round_trip() {
    const convert::Request decrypt_request{
        CodeFormat::action_replay_max, CodeFormat::armax_raw,
        devices::action_replay::ar1_seed,
        devices::gameshark::default_key,
        devices::armax::default_payload_key};
    const convert::Request encrypt_request{
        CodeFormat::armax_raw, CodeFormat::action_replay_max,
        devices::action_replay::ar1_seed,
        devices::gameshark::default_key,
        devices::armax::default_payload_key};

    const auto verify_round_trip = [&](const std::string& encrypted_text,
                                       const std::string& escaped_raw_label) {
        const convert::Result raw_result =
            convert::convert_text(encrypted_text, decrypt_request);
        require(raw_result.output_text.find(escaped_raw_label) != std::string::npos,
                "ARMAX RAW writer did not escape a parser-reserved cheat name");

        const convert::Result encrypted_result =
            convert::convert_text(raw_result.output_text, encrypt_request);
        require(crypto_vectors::parse_armax_lines(encrypted_result.output_text) ==
                    crypto_vectors::parse_armax_lines(encrypted_text),
                "ARMAX encrypted lines changed after a reserved-name round trip");
    };

    verify_round_trip(
        "+Free Costume Doors Activated by Codejunkies , CRYPT_ARMAX\n"
        "RDBQ-3PU3-QGCZ9\n"
        "9NRW-WPX8-J8C44\n"
        "+$500 Granted by Codejunkies , CRYPT_ARMAX\n"
        "RHPV-MB84-4T7G6\n"
        "36YB-P7BH-5K5D6\n",
        "+$500 Granted by Codejunkies , CRYPT_MAXRAW");

    verify_round_trip(
        "+Days Played by Codejunkies , CRYPT_ARMAX\n"
        "VMPD-VT56-1D2NB\n"
        "HEFZ-N9WZ-8JNBQ\n"
        "2Z2E-U14R-193U1\n"
        "+# of Days Passed: by Codejunkies , CRYPT_ARMAX\n"
        "JM0K-6F2U-6H1H9\n"
        "8UQH-1851-EZQPC\n",
        "+# of Days Passed: by Codejunkies , CRYPT_MAXRAW");
}

void test_strict_encrypted_parser() {
    bool threw = false;
    try {
        (void)formats::text::parse_text(
            "Name\n1234-5678-ABCDI\n", Crypt::armax);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "ARMAX encrypted parser accepted an invalid alphabet character");

    threw = false;
    try {
        (void)formats::text::parse_text(
            "Name\n00123456 89ABCDEF\n", Crypt::armax);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "ARMAX encrypted parser silently accepted a RAW code row");
}

} // namespace

void run_armax_tests() {
    test_alphabet_codec();
    test_supplied_vectors();
    test_text_conversion_service();
    test_reserved_label_round_trip();
    test_strict_encrypted_parser();
}

} // namespace omni::tests
