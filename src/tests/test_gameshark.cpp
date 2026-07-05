#include "tests/test_gameshark.hpp"

#include "convert/conversion_service.hpp"
#include "devices/gameshark/gs3_crypto.hpp"
#include "devices/gameshark/gs3_verifier.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/crypto/vector_file.hpp"
#include "tests/test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifndef OMNI_TEST_VECTOR_DIR
#error OMNI_TEST_VECTOR_DIR must be defined for crypto vector tests
#endif

namespace omni::tests {
namespace {

const std::filesystem::path vector_root{OMNI_TEST_VECTOR_DIR};

std::vector<std::vector<std::uint32_t>> non_empty_block_words(
    const std::vector<formats::text::CheatBlock>& blocks) {
    std::vector<std::vector<std::uint32_t>> result;
    for (const auto& block : blocks) {
        if (!block.cheat.empty()) result.push_back(block.cheat.words);
    }
    return result;
}

std::vector<std::vector<std::uint32_t>> load_blocks(const char* file_name) {
    return non_empty_block_words(formats::text::parse_text(
        crypto_vectors::read_text(vector_root / file_name)));
}

void verify_golden_file_pair(const char* decrypted_name,
                             const char* encrypted_name,
                             std::uint8_t key,
                             std::size_t expected_raw_pairs,
                             std::size_t expected_blocks) {
    const auto decrypted_blocks = load_blocks(decrypted_name);
    const auto encrypted_blocks = load_blocks(encrypted_name);

    require(decrypted_blocks.size() == expected_blocks,
            "GameShark decrypted cheat block count changed");
    require(encrypted_blocks.size() == decrypted_blocks.size(),
            "GameShark encrypted/decrypted cheat block counts differ");

    std::size_t pair_count = 0U;
    for (std::size_t block = 0; block < decrypted_blocks.size(); ++block) {
        auto generated = decrypted_blocks[block];
        devices::gameshark::Context encrypt_context;
        encrypt_context.encrypt_words(generated, key);
        devices::gameshark::add_verifier(generated);

        require(generated == encrypted_blocks[block],
                "GameShark/Xploder 5+ encryption differs from supplied vectors");

        devices::gameshark::Context decrypt_context;
        decrypt_context.decrypt_words(generated, true);
        require(generated == decrypted_blocks[block],
                "GameShark/Xploder 5+ decryption differs from supplied vectors");

        pair_count += generated.size() / 2U;
    }

    require(pair_count == expected_raw_pairs,
            "GameShark supplied decrypted vector count changed");
}

void test_4x4_evo2_key2_vectors() {
    verify_golden_file_pair(
        "Xploderv5+[Decrypted]-4x4 Evo 2.txt",
        "Xploderv5+[Encrypted]-4x4 Evo 2.txt",
        2U, 2U, 2U);
}

void test_catwoman_key4_vectors() {
    verify_golden_file_pair(
        "Xploderv5+[Decrypted]-Catwoman - Copy.txt",
        "Xploderv5+[Encrypted]-Catwoman.txt",
        4U, 8U, 6U);
}

void test_known_lines_and_verifiers() {
    devices::gameshark::Context key2;
    require(key2.encrypt({0xF0100008U, 0x00299B97U}, 2U) ==
                CodePair{0xF457BE3CU, 0xB67C0D16U},
            "GameShark key-2 master-code line differs from supplied vector");

    devices::gameshark::Context key4;
    require(key4.encrypt({0x901D2280U, 0x0C078881U}, 4U) ==
                CodePair{0x98075E5AU, 0x789BE306U},
            "GameShark key-4 master-code line differs from supplied vector");

    std::vector<std::uint32_t> encrypted{
        0xF457BE3CU, 0xB67C0D16U};
    require(devices::gameshark::create_verifier(encrypted) ==
                CodePair{0x76032730U, 0x00000000U},
            "GameShark v5 verifier differs from supplied vector");
}

void test_single_line_keys_round_trip() {
    for (std::uint8_t key = 0U; key <= 4U; ++key) {
        const CodePair original{0x20123456U, 0x89ABCDEFU};
        devices::gameshark::Context encrypt_context;
        const CodePair encrypted = encrypt_context.encrypt(original, key);

        devices::gameshark::Context decrypt_context;
        const CodePair decrypted = decrypt_context.decrypt(encrypted);
        require(decrypted == original,
                "GameShark single-line key round trip failed");
    }
}

void test_two_line_keys_round_trip() {
    for (std::uint8_t key = 1U; key <= 4U; ++key) {
        std::vector<std::uint32_t> words{
            0x30123456U, 0x00000005U,
            0x01234567U, 0x89ABCDEFU,
        };
        const auto original = words;

        devices::gameshark::Context encrypt_context;
        encrypt_context.encrypt_words(words, key);

        devices::gameshark::Context decrypt_context;
        decrypt_context.decrypt_words(words, false);
        require(words == original,
                "GameShark two-line key round trip failed");
    }
}

void test_text_conversion_service() {
    const std::string raw_4x4 = crypto_vectors::read_text(
        vector_root / "Xploderv5+[Decrypted]-4x4 Evo 2.txt");
    const std::string encrypted_4x4 = crypto_vectors::read_text(
        vector_root / "Xploderv5+[Encrypted]-4x4 Evo 2.txt");

    const convert::Request encrypt_request{
        CodeFormat::standard_raw, CodeFormat::gameshark_madcatz5,
        devices::action_replay::ar1_seed, 2U};
    const convert::Result encrypted_result =
        convert::convert_text(raw_4x4, encrypt_request);
    require(non_empty_block_words(formats::text::parse_text(encrypted_result.output_text)) ==
                non_empty_block_words(formats::text::parse_text(encrypted_4x4)),
            "GameShark v5 text encryption differs from supplied vectors");

    const convert::Request decrypt_request{
        CodeFormat::gameshark_madcatz5, CodeFormat::standard_raw,
        devices::action_replay::ar1_seed, 4U};
    const convert::Result decrypted_result =
        convert::convert_text(encrypted_4x4, decrypt_request);
    require(non_empty_block_words(formats::text::parse_text(decrypted_result.output_text)) ==
                non_empty_block_words(formats::text::parse_text(raw_4x4)),
            "GameShark v5 text decryption differs from supplied vectors");
}

void test_gs3_output_has_no_verifier() {
    const auto raw_blocks = load_blocks(
        "Xploderv5+[Decrypted]-4x4 Evo 2.txt");
    const auto supplied_v5_blocks = load_blocks(
        "Xploderv5+[Encrypted]-4x4 Evo 2.txt");

    require(raw_blocks.size() == supplied_v5_blocks.size(),
            "GameShark no-verifier test block counts differ");

    for (std::size_t i = 0; i < raw_blocks.size(); ++i) {
        auto expected = supplied_v5_blocks[i];
        expected.erase(expected.begin(), expected.begin() + 2);

        auto generated = raw_blocks[i];
        devices::gameshark::Context context;
        context.encrypt_words(generated, 2U);
        require(generated == expected,
                "GameShark 3/4 output should match encrypted payload without v5 verifier");
    }
}

void test_gs3_type7_payload_rows_survive_round_trip() {
    const std::vector<std::uint32_t> original{
        0x76035519U, 0x1BA991C8U,
        0x45943278U, 0xBAF4D558U,
        0xFF95FFFFU, 0x00000000U,
    };

    auto decrypted = original;
    devices::gameshark::Context decrypt_context;
    decrypt_context.decrypt_words(decrypted, true);

    require(decrypted.size() == original.size(),
            "GS3 type-7 payload row was mistaken for a V5 verifier");
    require(decrypted[0] == original[0] && decrypted[1] == original[1],
            "GS3 type-7 payload row changed during decryption");

    devices::gameshark::Context encrypt_context;
    encrypt_context.encrypt_words(decrypted, 2U);
    require(decrypted == original,
            "GS3 type-7 payload block failed an encrypted round trip");
}

void test_gs5_verifier_and_type7_payload_rows_survive_round_trip() {
    const std::vector<std::uint32_t> original{
        0x760A9920U, 0x00000000U,
        0x77000183U, 0x66EC9AC5U,
        0x980E20FBU, 0x78D0F8AFU,
    };

    require(devices::gameshark::has_valid_verifier(original),
            "Supplied GS5 type-7 payload vector should have a valid verifier");

    auto decrypted = original;
    devices::gameshark::Context decrypt_context;
    decrypt_context.decrypt_words(decrypted, true);
    require(decrypted.size() == original.size() - 2U,
            "GS5 verifier was not removed exactly once");
    require(decrypted[0] == original[2] && decrypted[1] == original[3],
            "GS5 payload type-7 row was removed or changed");

    devices::gameshark::Context encrypt_context;
    encrypt_context.encrypt_words(decrypted, 4U);
    devices::gameshark::add_verifier(decrypted);
    require(decrypted == original,
            "GS5 verifier-plus-type-7 block failed an encrypted round trip");
}

void test_invalid_gs5_verifier_is_detectable_and_corrected() {
    const std::vector<std::uint32_t> original{
        0x7605F510U, 0x00000000U,
        0x04D01468U, 0x3470517CU,
        0x04803D6AU, 0x347053DCU,
    };

    require(devices::gameshark::looks_like_verifier(
                {original[0], original[1]}),
            "Malformed GS5 verifier should retain verifier structure");
    require(!devices::gameshark::has_valid_verifier(original),
            "Malformed GS5 verifier unexpectedly passed its CRC check");

    auto corrected = original;
    devices::gameshark::Context decrypt_context;
    decrypt_context.decrypt_words(corrected, true);
    devices::gameshark::Context encrypt_context;
    encrypt_context.encrypt_words(corrected, 2U);
    devices::gameshark::add_verifier(corrected);

    require(corrected[0] == 0x7607A630U && corrected[1] == 0U,
            "Malformed GS5 verifier was not regenerated from the payload");
    require(std::equal(corrected.begin() + 2, corrected.end(),
                       original.begin() + 2),
            "Correcting a GS5 verifier changed the encrypted payload");

    const std::string text =
        "+Player 2 Always Wins by Xploder, CRYPT_GS3\n"
        "7605F510 00000000\n"
        "04D01468 3470517C\n"
        "04803D6A 347053DC\n";
    const convert::Request request{
        CodeFormat::gameshark_madcatz5,
        CodeFormat::gameshark3_raw,
        devices::action_replay::ar1_seed,
        2U};
    const convert::Result result = convert::convert_text(text, request);
    require(result.warnings.size() == 1U,
            "Malformed GS5 verifier should produce one checksum warning");
    require(result.warnings.front().find("verifier checksum is invalid") !=
                std::string::npos,
            "Malformed GS5 verifier warning text changed unexpectedly");
}

} // namespace

void run_gameshark_tests() {
    test_4x4_evo2_key2_vectors();
    test_catwoman_key4_vectors();
    test_known_lines_and_verifiers();
    test_single_line_keys_round_trip();
    test_two_line_keys_round_trip();
    test_text_conversion_service();
    test_gs3_output_has_no_verifier();
    test_gs3_type7_payload_rows_survive_round_trip();
    test_gs5_verifier_and_type7_payload_rows_survive_round_trip();
    test_invalid_gs5_verifier_is_detectable_and_corrected();
}

} // namespace omni::tests
