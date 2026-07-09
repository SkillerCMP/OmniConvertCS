#include "tests/test_options.hpp"

#include "convert/conversion_service.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "devices/armax/armax_crypto.hpp"
#include "devices/armax/armax_options.hpp"
#include "devices/codebreaker/cb_batch.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "tests/test_support.hpp"

#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace omni::tests {
namespace {


void require_block_words(const convert::Result& result,
                         std::initializer_list<std::uint32_t> expected,
                         const char* message) {
    require(result.blocks.size() == 1U, message);
    const auto& words = result.blocks.front().cheat.words;
    require(words.size() == expected.size(), message);
    std::size_t index = 0U;
    for (const std::uint32_t value : expected) {
        require(words[index] == value, message);
        ++index;
    }
}

std::string encrypted_cb7_text(std::vector<std::uint32_t> words) {
    devices::codebreaker::encrypt_common_v7(words);
    std::ostringstream output;
    output << "MGS Pointer\n" << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t index = 0U; index + 1U < words.size(); index += 2U) {
        output << std::setw(8) << words[index] << ' '
               << std::setw(8) << words[index + 1U] << '\n';
    }
    return output.str();
}

void test_ar2_common_key_codes() {
    using namespace devices::action_replay;
    constexpr std::uint8_t type = 0x05U;
    constexpr std::uint8_t seed = 0x18U;

    require(encrypt_word(0x04030209U, type, seed) == 0x1853E59EU,
            "AR2 common key 1853E59E no longer maps to seed 04030209");
    require(decrypt_word(0x1853E59EU, type, seed) == 0x04030209U,
            "AR2 common key 1853E59E did not decrypt to seed 04030209");
    require(encrypt_word(decrypt_word(0x1645EBB3U, type, seed), type, seed) ==
                0x1645EBB3U,
            "AR2 common key 1645EBB3 did not round-trip");
    require(encrypt_word(decrypt_word(0x1746EAADU, type, seed), type, seed) ==
                0x1746EAADU,
            "AR2 common key 1746EAAD did not round-trip");
}

convert::Result make_armax_raw() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::armax_raw;
    request.armax_game_id = 0x1234U;
    request.armax_region = 1U;
    request.armax_verifier_mode = convert::ArmaxVerifierMode::automatic;
    return convert::convert_text("Test Code\n20100000 DEADBEEF\n", request);
}

void test_armax_verifier_modes() {
    const convert::Result armax_raw = make_armax_raw();
    require(armax_raw.blocks.size() == 1U,
            "ARMAX verifier setup produced an unexpected block count");
    require(armax_raw.blocks.front().cheat.words.size() == 4U,
            "Automatic ARMAX verifier was not added to standard input");

    convert::Request manual_leave;
    manual_leave.input_format = CodeFormat::armax_raw;
    manual_leave.output_format = CodeFormat::standard_raw;
    manual_leave.armax_verifier_mode = convert::ArmaxVerifierMode::manual;
    const convert::Result stripped =
        convert::convert_text(armax_raw.output_text, manual_leave);
    require(stripped.blocks.size() == 1U &&
                stripped.blocks.front().cheat.words.size() == 2U,
            "Manual ARMAX verifier mode did not strip the supplied verifier");
    require(stripped.blocks.front().cheat.words[0] == 0x20100000U &&
                stripped.blocks.front().cheat.words[1] == 0xDEADBEEFU,
            "Manual ARMAX verifier removal changed the translated payload");

    convert::Request manual_encrypt;
    manual_encrypt.input_format = CodeFormat::armax_raw;
    manual_encrypt.output_format = CodeFormat::action_replay_max;
    manual_encrypt.armax_verifier_mode = convert::ArmaxVerifierMode::manual;
    const convert::Result encrypted_once =
        convert::convert_text(armax_raw.output_text, manual_encrypt);
    std::vector<std::uint32_t> decrypted_once =
        encrypted_once.blocks.front().cheat.words;
    const devices::armax::Metadata once_metadata =
        devices::armax::decrypt_full(decrypted_once);
    require(once_metadata.game_id == 0x1234U && once_metadata.region == 1U,
            "Manual ARMAX output did not retain verifier metadata");
    require(decrypted_once.size() == 4U,
            "Manual ARMAX output duplicated an existing verifier");

    convert::Request automatic_encrypt = manual_encrypt;
    automatic_encrypt.armax_verifier_mode =
        convert::ArmaxVerifierMode::automatic;
    automatic_encrypt.armax_game_id = 0x0123U;
    automatic_encrypt.armax_region = 2U;
    const convert::Result encrypted_twice =
        convert::convert_text(armax_raw.output_text, automatic_encrypt);
    std::vector<std::uint32_t> decrypted_twice =
        encrypted_twice.blocks.front().cheat.words;
    const devices::armax::Metadata twice_metadata =
        devices::armax::decrypt_full(decrypted_twice);
    require(twice_metadata.game_id == 0x0123U && twice_metadata.region == 2U,
            "Automatic ARMAX output did not generate new verifier metadata");
    require(decrypted_twice.size() == 6U,
            "Automatic ARMAX output did not prepend a verifier to MAX RAW input");
}


void test_armax_random_verifier_refresh() {
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::armax_raw;
    request.armax_game_id = 0x1234U;
    request.armax_region = 1U;
    request.armax_verifier_mode = convert::ArmaxVerifierMode::automatic;

    const convert::Result first =
        convert::convert_text("Test Code\n20100000 DEADBEEF\n", request);
    const convert::Result second =
        convert::convert_text("Test Code\n20100000 DEADBEEF\n", request);

    require(first.blocks.size() == 1U && second.blocks.size() == 1U,
            "ARMAX random verifier test produced an unexpected block count");
    require(first.blocks[0].cheat.words.size() == 4U &&
                second.blocks[0].cheat.words.size() == 4U,
            "ARMAX random verifier test did not generate headers");
    require(first.blocks[0].cheat.words[0] != second.blocks[0].cheat.words[0] ||
                first.blocks[0].cheat.words[1] != second.blocks[0].cheat.words[1],
            "ARMAX automatic verifier did not change between conversions");

    request.armax_verifier_nonce = 0x4567U;
    const convert::Result deterministic_first =
        convert::convert_text("Test Code\n20100000 DEADBEEF\n", request);
    const convert::Result deterministic_second =
        convert::convert_text("Test Code\n20100000 DEADBEEF\n", request);
    require(deterministic_first.output_text == deterministic_second.output_text,
            "Explicit ARMAX verifier nonce is no longer deterministic");
}

void test_armax_disc_hash_crc() {
    const std::vector<std::uint8_t> system_cnf{
        '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const std::vector<std::uint8_t> elf_prefix{'a', 'b', 'c'};
    require(devices::armax::compute_disc_hash(system_cnf, elf_prefix) ==
                0xFED078E4U,
            "ARMAX disc hash CRC no longer matches OmniConvertCS");
}

void test_armax_organizers_and_disc_hash() {
    std::uint32_t folder_id = 0U;
    Cheat folder;
    folder.name = "Characters";
    devices::armax::make_folder(folder.words, 0x0357U, 0U, 0x12345U);
    devices::armax::finalize_cheat(folder, 0U, true, folder_id);
    require(folder_id != 0U && folder.id == folder_id,
            "ARMAX organizer did not establish the current folder ID");
    require((folder.words[1] & devices::armax::flags_folder) != 0U,
            "ARMAX organizer did not receive folder flags");

    Cheat member;
    member.name = "Max Level";
    member.words = {0x00000000U, 0x00800000U,
                    0x049F9FCCU, 0x000003E7U};
    devices::armax::finalize_cheat(member, 0U, true, folder_id);
    require((member.words[1] & devices::armax::flags_folder_member) != 0U,
            "ARMAX organizer member flag was not applied");
    require((member.words[1] & 1U) != 0U,
            "ARMAX organizer member link bit was not applied");

    Cheat master;
    master.name = "(M)";
    master.words = {0x00000000U, 0x08800000U,
                    0xC4145714U, 0x0003FF00U};
    std::uint32_t no_folder = 0U;
    constexpr std::uint32_t disc_hash = 0x89ABCDEFU;
    devices::armax::finalize_cheat(master, disc_hash, false, no_folder);
    require(master.flags[Cheat::flag_master_code] != 0U,
            "ARMAX C4 hook was not recognized as a master code");
    require(master.words.size() == 6U,
            "ARMAX disc hash did not add its expansion pair");
    require((master.words[1] & devices::armax::flags_disc_hash) != 0U,
            "ARMAX disc hash flags were not applied");
    require(master.words[2] ==
                (0x00080000U | (disc_hash << 20U)) &&
                master.words[3] == 0U,
            "ARMAX disc hash expansion data differs from OmniConvertCS");
}

void test_make_organizers_conversion_path() {
    const std::string source =
        "!Characters:\n"
        "+Max Level\n"
        "209F9FCC 000003E7\n"
        "!!\n";

    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::armax_raw;
    request.armax_game_id = 0x0357U;
    request.make_organizers = true;
    request.armax_verifier_mode = convert::ArmaxVerifierMode::automatic;
    const convert::Result result = convert::convert_text(source, request);

    require(result.blocks.size() >= 2U,
            "Make Organizers removed the organizer or its member");
    require(!result.blocks[0].cheat.empty(),
            "Make Organizers did not convert a name-only block into a folder");
    require((result.blocks[0].cheat.words[1] & devices::armax::flags_folder) != 0U,
            "Converted organizer is missing ARMAX folder flags");
    require((result.blocks[1].cheat.words[1] &
             devices::armax::flags_folder_member) != 0U,
            "Following code was not linked to the converted organizer");
}


void test_mgs_c_type_pointer_mode() {
    convert::Request cb_to_cb;
    cb_to_cb.input_format = CodeFormat::codebreaker_raw;
    cb_to_cb.output_format = CodeFormat::codebreaker_raw;
    cb_to_cb.mgs_c_type_pointer_mode = true;
    const convert::Result cb_result = convert::convert_text(
        "MGS Pointer\nC00C0000 02B803E7\n", cb_to_cb);
    require_block_words(cb_result, {
        0x600C0000U, 0x000003E7U,
        0x00010001U, 0x000002B8U,
    }, "MGS C-type pointer did not normalize to CodeBreaker type 6");

    convert::Request cb_to_gs = cb_to_cb;
    cb_to_gs.output_format = CodeFormat::gameshark3_raw;
    const convert::Result gs_result = convert::convert_text(
        "MGS Pointer\nC00C0000 02B803E7\n", cb_to_gs);
    require_block_words(gs_result, {
        0x610C0000U, 0x00000000U,
        0x000002B8U, 0x000003E7U,
    }, "MGS C-type pointer did not translate to GameShark pointer format");

    convert::Request encrypted;
    encrypted.input_format = CodeFormat::codebreaker7_common;
    encrypted.output_format = CodeFormat::codebreaker_raw;
    encrypted.mgs_c_type_pointer_mode = true;
    const convert::Result encrypted_result = convert::convert_text(
        encrypted_cb7_text({0xC00C0000U, 0x02B803E7U}), encrypted);
    require_block_words(encrypted_result, {
        0x600C0000U, 0x000003E7U,
        0x00010001U, 0x000002B8U,
    }, "Encrypted CodeBreaker input was not normalized after decryption");

    convert::Request disabled;
    disabled.input_format = CodeFormat::codebreaker_raw;
    disabled.output_format = CodeFormat::codebreaker_raw;
    const convert::Result disabled_result = convert::convert_text(
        "MGS Pointer\nC00C0000 02B803E7\n", disabled);
    require_block_words(disabled_result, {
        0xC00C0000U, 0x02B803E7U,
    }, "MGS C-type pointer changed while the option was disabled");
}

} // namespace

void run_option_tests() {
    test_ar2_common_key_codes();
    test_armax_verifier_modes();
    test_armax_random_verifier_refresh();
    test_armax_disc_hash_crc();
    test_armax_organizers_and_disc_hash();
    test_make_organizers_conversion_path();
    test_mgs_c_type_pointer_mode();
}

} // namespace omni::tests
