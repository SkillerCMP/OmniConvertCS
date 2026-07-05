#include "tests/test_codebreaker.hpp"

#include "convert/conversion_service.hpp"
#include "devices/codebreaker/cb1_crypto.hpp"
#include "devices/codebreaker/cb_batch.hpp"
#include "devices/codebreaker/cb7_crypto.hpp"
#include "devices/codebreaker/cb7_math.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/crypto/vector_file.hpp"
#include "tests/test_support.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

#ifndef OMNI_TEST_VECTOR_DIR
#error OMNI_TEST_VECTOR_DIR must be defined for crypto vector tests
#endif

namespace omni::tests {
namespace {

const std::filesystem::path vector_root{OMNI_TEST_VECTOR_DIR};

std::vector<std::uint32_t> flatten(const std::vector<CodePair>& pairs) {
    std::vector<std::uint32_t> words;
    words.reserve(pairs.size() * 2U);
    for (const CodePair& pair : pairs) {
        words.push_back(pair.address);
        words.push_back(pair.value);
    }
    return words;
}

std::vector<std::uint32_t> load_hex_words(const char* file_name) {
    return flatten(crypto_vectors::parse_hex_pairs(
        crypto_vectors::read_text(vector_root / file_name)));
}

std::vector<std::uint32_t> flatten_blocks(
    const std::vector<formats::text::CheatBlock>& blocks) {
    std::vector<std::uint32_t> words;
    for (const auto& block : blocks) {
        words.insert(words.end(), block.cheat.words.begin(), block.cheat.words.end());
    }
    return words;
}

void test_full_golden_vectors() {
    using devices::codebreaker::decrypt_v1;
    using devices::codebreaker::encrypt_v1;

    const auto decrypted = load_hex_words(
        "CB1-6[Decryption]-4 x 4 Evolution - SLUS_200.91.txt");
    const auto encrypted = load_hex_words(
        "CB1-6[Encryption]-4 x 4 Evolution - SLUS_200.91.txt");

    require(decrypted.size() == 18U, "CodeBreaker 1-6 decrypted vector count changed");
    require(encrypted.size() == decrypted.size(), "CodeBreaker 1-6 vector sizes differ");

    auto generated = decrypted;
    encrypt_v1(generated);
    require(generated == encrypted,
            "CodeBreaker 1-6 encryption does not match supplied vectors");

    decrypt_v1(generated);
    require(generated == decrypted,
            "CodeBreaker 1-6 decryption does not match supplied vectors");
}

void test_all_command_nibbles_round_trip() {
    using devices::codebreaker::decrypt_v1;
    using devices::codebreaker::encrypt_v1;

    for (std::uint32_t command = 0U; command < 16U; ++command) {
        const CodePair original{
            (command << 28U) | 0x00123456U,
            0x89ABCDEFU ^ (command * 0x01010101U),
        };
        const CodePair encrypted = encrypt_v1(original);
        const CodePair decrypted = decrypt_v1(encrypted);
        require(decrypted == original,
                "CodeBreaker 1-6 command-nibble round trip failed");
    }
}

void test_text_conversion_service() {
    const std::string encrypted_text = crypto_vectors::read_text(
        vector_root / "CB1-6[Encryption]-4 x 4 Evolution - SLUS_200.91.txt");
    const std::string decrypted_text = crypto_vectors::read_text(
        vector_root / "CB1-6[Decryption]-4 x 4 Evolution - SLUS_200.91.txt");

    const convert::Request decrypt_request{CodeFormat::codebreaker1_6, CodeFormat::standard_raw, 0U};
    const convert::Result decrypted_result = convert::convert_text(encrypted_text, decrypt_request);
    const auto decoded_blocks = formats::text::parse_text(decrypted_result.output_text);
    const auto expected_blocks = formats::text::parse_text(decrypted_text);

    require(flatten_blocks(decoded_blocks) == flatten_blocks(expected_blocks),
            "CodeBreaker text decryption differs from supplied vectors");

    const convert::Request encrypt_request{CodeFormat::standard_raw, CodeFormat::codebreaker1_6, 0U};
    const convert::Result encrypted_result = convert::convert_text(decrypted_text, encrypt_request);
    const auto encoded_blocks = formats::text::parse_text(encrypted_result.output_text);
    const auto supplied_blocks = formats::text::parse_text(encrypted_text);

    require(flatten_blocks(encoded_blocks) == flatten_blocks(supplied_blocks),
            "CodeBreaker text encryption differs from supplied vectors");
}


std::vector<std::vector<std::uint32_t>> non_empty_block_words(
    const std::vector<formats::text::CheatBlock>& blocks) {
    std::vector<std::vector<std::uint32_t>> result;
    for (const auto& block : blocks) {
        if (!block.cheat.empty()) result.push_back(block.cheat.words);
    }
    return result;
}

void test_cb7_common_first_pair() {
    devices::codebreaker::V7Context context;
    context.set_common_v7();

    const CodePair encrypted = context.encrypt(CodePair{0xFE129B51U, 0x800813BFU});
    require(encrypted == CodePair{0xB4336FA9U, 0x4DFEFB79U},
            "CodeBreaker 7+ common encryption first pair differs from supplied vector");

    devices::codebreaker::V7Context decrypt_context;
    decrypt_context.set_common_v7();
    require(decrypt_context.decrypt(encrypted) == CodePair{0xFE129B51U, 0x800813BFU},
            "CodeBreaker 7+ common first-pair decryption failed");
}

void test_cb7_rsa_round_trip() {
    constexpr std::array<CodePair, 5> values{{
        {0x00000000U, 0x00000000U},
        {0x00000000U, 0x00000001U},
        {0x12345678U, 0x9ABCDEF0U},
        {0xBEEFC0DEU, 0x10203040U},
        {0xFFFFFFFEU, 0xFFFFFFF4U},
    }};

    for (const CodePair original : values) {
        CodePair transformed = original;
        devices::codebreaker::cb7_math::rsa_transform(
            transformed.address, transformed.value,
            devices::codebreaker::cb7_math::rsa_encrypt_exponent);
        devices::codebreaker::cb7_math::rsa_transform(
            transformed.address, transformed.value,
            devices::codebreaker::cb7_math::rsa_decrypt_exponent);
        require(transformed == original, "CodeBreaker 7+ RSA round trip failed");
    }
}

void test_cb7_common_golden_vectors() {
    const std::string decrypted_text = crypto_vectors::read_text(
        vector_root / "CB7+[Decryption]-187 Ride Or Die - SLUS_211.16.txt");
    const std::string encrypted_text = crypto_vectors::read_text(
        vector_root / "CB7+[Encryption]-187 Ride Or Die - SLUS_211.16.txt");

    const auto decrypted_blocks = non_empty_block_words(formats::text::parse_text(decrypted_text));
    const auto encrypted_blocks = non_empty_block_words(formats::text::parse_text(encrypted_text));

    require(decrypted_blocks.size() == encrypted_blocks.size(),
            "CodeBreaker 7+ supplied cheat block counts differ");

    std::size_t total_pairs = 0U;
    for (std::size_t block = 0U; block < decrypted_blocks.size(); ++block) {
        auto generated = decrypted_blocks[block];
        devices::codebreaker::encrypt_common_v7(generated);
        require(generated == encrypted_blocks[block],
                "CodeBreaker 7+ common encryption differs from supplied vectors");

        devices::codebreaker::decrypt_common_v7(generated);
        require(generated == decrypted_blocks[block],
                "CodeBreaker 7+ common decryption differs from supplied vectors");
        total_pairs += generated.size() / 2U;
    }

    require(total_pairs == 111U, "CodeBreaker 7+ golden vector count changed");
}

void test_cb7_text_conversion_service() {
    const std::string encrypted_text = crypto_vectors::read_text(
        vector_root / "CB7+[Encryption]-187 Ride Or Die - SLUS_211.16.txt");
    const std::string decrypted_text = crypto_vectors::read_text(
        vector_root / "CB7+[Decryption]-187 Ride Or Die - SLUS_211.16.txt");

    const convert::Request decrypt_request{
        CodeFormat::codebreaker7_common, CodeFormat::standard_raw, 0U};
    const convert::Result decrypted_result =
        convert::convert_text(encrypted_text, decrypt_request);
    require(non_empty_block_words(formats::text::parse_text(decrypted_result.output_text)) ==
                non_empty_block_words(formats::text::parse_text(decrypted_text)),
            "CodeBreaker 7+ text decryption differs from supplied vectors");

    const convert::Request encrypt_request{
        CodeFormat::standard_raw, CodeFormat::codebreaker7_common, 0U};
    const convert::Result encrypted_result =
        convert::convert_text(decrypted_text, encrypt_request);
    require(non_empty_block_words(formats::text::parse_text(encrypted_result.output_text)) ==
                non_empty_block_words(formats::text::parse_text(encrypted_text)),
            "CodeBreaker 7+ text encryption differs from supplied vectors");
}

void test_cb1_wildcard_modifier_round_trip() {
    const std::string encrypted_text =
        "+Command 0 Modifier by Code Master , CRYPT_CB\n"
        "0A1F2229 000000??\n"
        "+Command 1 Modifier by Code Master , CRYPT_CB\n"
        "1AD12568 000000??\n"
        "+Command 2 Modifier by Code Master , CRYPT_CB\n"
        "2AC70FE0 00??????\n";

    const convert::Request decrypt_request{
        CodeFormat::codebreaker1_6, CodeFormat::codebreaker_raw, 0U};
    const convert::Result decrypted =
        convert::convert_text(encrypted_text, decrypt_request);
    require(decrypted.warnings.empty(),
            "CodeBreaker V1-V6 passthrough wildcards produced warnings");
    require(decrypted.output_text.find("000000??") != std::string::npos,
            "CodeBreaker V1-V6 two-nibble wildcard was removed");
    require(decrypted.output_text.find("00??????") != std::string::npos,
            "CodeBreaker V1-V6 six-nibble wildcard was removed");

    const convert::Request encrypt_request{
        CodeFormat::codebreaker_raw, CodeFormat::codebreaker1_6, 0U};
    const convert::Result encrypted =
        convert::convert_text(decrypted.output_text, encrypt_request);
    require(encrypted.warnings.empty(),
            "CodeBreaker V1-V6 wildcard re-encryption produced warnings");

    const auto supplied_blocks = formats::text::parse_text(
        encrypted_text, Crypt::codebreaker);
    const auto generated_blocks = formats::text::parse_text(
        encrypted.output_text, Crypt::codebreaker);
    require(flatten_blocks(generated_blocks) == flatten_blocks(supplied_blocks),
            "CodeBreaker V1-V6 wildcard code words changed after round trip");
    require(encrypted.output_text.find("0A1F2229 000000??") != std::string::npos,
            "CodeBreaker command-0 wildcard did not round trip exactly");
    require(encrypted.output_text.find("1AD12568 000000??") != std::string::npos,
            "CodeBreaker command-1 wildcard did not round trip exactly");
    require(encrypted.output_text.find("2AC70FE0 00??????") != std::string::npos,
            "CodeBreaker command-2 wildcard did not round trip exactly");
}

void test_code_looking_label_round_trip() {
    const std::string encrypted_text =
        "+1AA71663 00000909 by Code Master , CRYPT_CB\n"
        "2A5B0488 09090909\n";

    const convert::Request decrypt_request{
        CodeFormat::codebreaker1_6, CodeFormat::codebreaker_raw, 0U};
    const convert::Result decrypted =
        convert::convert_text(encrypted_text, decrypt_request);
    require(decrypted.output_text.find(
                "+1AA71663 00000909 by Code Master , CRYPT_CRAW") !=
                std::string::npos,
            "Code-looking cheat label was not escaped in CRAW output");

    const convert::Request encrypt_request{
        CodeFormat::codebreaker_raw, CodeFormat::codebreaker1_6, 0U};
    const convert::Result encrypted =
        convert::convert_text(decrypted.output_text, encrypt_request);
    require(encrypted.output_text.find(
                "+1AA71663 00000909 by Code Master , CRYPT_CB") !=
                std::string::npos,
            "Code-looking cheat label changed after CB round trip");

    const auto blocks = formats::text::parse_text(
        encrypted.output_text, Crypt::codebreaker);
    require(non_empty_block_words(blocks).size() == 1U,
            "Code-looking label was parsed as an additional code line");
    require(non_empty_block_words(blocks).front().size() == 2U,
            "Code-looking label changed the cheat word count");
}

} // namespace

void run_codebreaker_tests() {
    test_full_golden_vectors();
    test_all_command_nibbles_round_trip();
    test_text_conversion_service();
    test_cb7_common_first_pair();
    test_cb7_rsa_round_trip();
    test_cb7_common_golden_vectors();
    test_cb7_text_conversion_service();
    test_cb1_wildcard_modifier_round_trip();
    test_code_looking_label_round_trip();
}

} // namespace omni::tests
