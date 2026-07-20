#include "tests/test_action_replay.hpp"

#include "convert/conversion_service.hpp"
#include "devices/action_replay/ar_batch.hpp"
#include "tests/crypto/vector_file.hpp"
#include "tests/test_support.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifndef OMNI_TEST_VECTOR_DIR
#error OMNI_TEST_VECTOR_DIR must be defined for crypto vector tests
#endif

namespace omni::tests {
namespace {

using devices::action_replay::Context;
using devices::action_replay::ar1_seed;
using devices::action_replay::decrypt_ar1;
using devices::action_replay::encrypt_ar1;
using devices::action_replay::prepend_ar2_key_code;
using devices::action_replay::seed_from_displayed_key;

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

void test_ar1_full_vectors() {
    const auto decrypted = load_hex_words("Arv1-Decrypted.txt");
    const auto encrypted = load_hex_words("Arv1-Encrypted.txt");
    require(decrypted.size() == 12U, "AR1 decrypted vector count changed");
    require(encrypted.size() == decrypted.size(), "AR1 vector sizes differ");

    auto generated = decrypted;
    encrypt_ar1(generated);
    require(generated == encrypted, "AR1 encryption does not match supplied vectors");

    decrypt_ar1(generated);
    require(generated == decrypted, "AR1 decryption round trip failed");
}

void test_ar2_keyed_full_vectors() {
    constexpr std::uint32_t decoded_key = 0x04030209U;
    const auto decrypted = load_hex_words("ARv2-Decrypted.txt");
    const auto encrypted = load_hex_words("ARv2-Encrypted.txt");
    require(decrypted.size() == 8U, "AR2 decrypted vector count changed");
    require(encrypted.size() == 10U, "AR2 encrypted vector count changed");

    auto decoded = encrypted;
    Context decrypt_context(ar1_seed);
    decrypt_context.decrypt_ar2(decoded, true);
    require(decoded == decrypted, "AR2 key-code decryption does not match supplied vectors");
    require(decrypt_context.get_seed() == decoded_key, "AR2 key line did not update the decoded seed");

    auto generated = decrypted;
    prepend_ar2_key_code(generated, decoded_key);
    Context encrypt_context(ar1_seed);
    encrypt_context.encrypt_ar2(generated);
    require(generated == encrypted, "AR2 keyed encryption does not match supplied vectors");
}


void test_ar2_key_persists_across_text_blocks() {
    const std::string encrypted_text =
        crypto_vectors::read_text(vector_root / "ARv2-Encrypted.txt");
    const auto expected = load_hex_words("ARv2-Decrypted.txt");

    convert::Request request;
    request.input_format = CodeFormat::action_replay2;
    request.output_format = CodeFormat::ar12_raw;

    const convert::Result result = convert::convert_text(encrypted_text, request);
    const auto actual = flatten(crypto_vectors::parse_hex_pairs(result.output_text));
    require(actual == expected,
            "AR2 decryption did not retain the key across named cheat blocks");
}

void test_ar2_key_is_emitted_once_across_text_blocks() {
    constexpr std::uint32_t decoded_key = 0x04030209U;
    const std::string decrypted_text =
        crypto_vectors::read_text(vector_root / "ARv2-Decrypted.txt");
    const auto expected = load_hex_words("ARv2-Encrypted.txt");

    convert::Request request;
    request.input_format = CodeFormat::ar12_raw;
    request.output_format = CodeFormat::action_replay2;
    request.ar2_key = decoded_key;

    const convert::Result result = convert::convert_text(decrypted_text, request);
    const auto actual = flatten(crypto_vectors::parse_hex_pairs(result.output_text));
    require(actual == expected,
            "AR2 encryption did not retain one key context across named cheat blocks");
}

void test_ar2_raw_enable_output_skips_encryption() {
    convert::Request request;
    request.input_format = CodeFormat::ar12_raw;
    request.output_format = CodeFormat::action_replay2;
    request.output_ar2_key = seed_from_displayed_key(0x1456E7A5U);

    const convert::Result result = convert::convert_text(
        "20123456 00000000\n", request);
    const auto actual = flatten(crypto_vectors::parse_hex_pairs(result.output_text));
    const std::vector<std::uint32_t> expected{
        0x0E3C7DF2U, 0x1456E7A5U,
        0x20123456U, 0x00000000U
    };
    require(actual == expected,
            "AR2 RAW-enable output should prepend 0E3C7DF2 1456E7A5 and skip encryption");
}


void test_ar2_displayed_header_is_not_converted_when_input_key_selected() {
    convert::Request request;
    request.input_format = CodeFormat::action_replay2;
    request.output_format = CodeFormat::action_replay2;
    request.input_ar2_key = seed_from_displayed_key(0x1853E59EU);
    request.output_ar2_key = seed_from_displayed_key(0x1456E7A5U);

    const convert::Result result = convert::convert_text(
        "!Master Codes:\n"
        "(M) Must Be On by Codejunkies\n"
        "0E3C7DF2 1853E59E\n"
        "EE8CDC1A BCBBBD2A\n"
        "!!\n",
        request);
    const auto actual = flatten(crypto_vectors::parse_hex_pairs(result.output_text));
    const std::vector<std::uint32_t> expected{
        0x0E3C7DF2U, 0x1456E7A5U,
        0xF01222A4U, 0x001222A7U
    };
    require(actual == expected,
            "AR2 displayed key header should be consumed as metadata, not converted as a code row");
}

void test_ar2_raw_enable_input_skips_decryption() {
    convert::Request request;
    request.input_format = CodeFormat::action_replay2;
    request.output_format = CodeFormat::ar12_raw;
    request.input_ar2_key = seed_from_displayed_key(0x1456E7A5U);

    const convert::Result result = convert::convert_text(
        "0E3C7DF2 1456E7A5\n20123456 00000000\n", request);
    const auto actual = flatten(crypto_vectors::parse_hex_pairs(result.output_text));
    const std::vector<std::uint32_t> expected{0x20123456U, 0x00000000U};
    require(actual == expected,
            "AR2 RAW-enable input should remove the enable marker and skip decryption");
}

} // namespace

void run_action_replay_tests() {
    test_ar1_full_vectors();
    test_ar2_keyed_full_vectors();
    test_ar2_key_persists_across_text_blocks();
    test_ar2_key_is_emitted_once_across_text_blocks();
    test_ar2_raw_enable_output_skips_encryption();
    test_ar2_raw_enable_input_skips_decryption();
    test_ar2_displayed_header_is_not_converted_when_input_key_selected();
}

} // namespace omni::tests
