#include "tests/test_armax_disc_hash.hpp"

#include "devices/armax/armax_disc_hash.hpp"
#include "devices/armax/armax_options.hpp"
#include "tests/test_support.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace omni::tests {
namespace {

class TempDirectory {
public:
    TempDirectory() {
        const auto nonce = std::chrono::high_resolution_clock::now()
                               .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("omniconvert-armax-disc-hash-" + std::to_string(nonce));
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

void test_system_cnf_boot_parser() {
    const auto boot = devices::armax::parse_system_cnf_boot_path(
        "BOOT2 = cdrom0:\\SLUS_204.97;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n");
    require(boot && *boot == "SLUS_204.97",
            "SYSTEM.CNF BOOT2 path was not parsed correctly");

    const auto quoted = devices::armax::parse_system_cnf_boot_path(
        "boot2 = \"cdrom0:/MODULES/GAME.ELF;1\"\n");
    require(quoted && *quoted == "MODULES/GAME.ELF",
            "Quoted/nested SYSTEM.CNF BOOT2 path was not parsed correctly");
}

void test_same_root_auto_elf_and_256k_limit() {
    TempDirectory temp;
    const std::filesystem::path cnf = temp.path() / "SYSTEM(1).CNF";
    const std::filesystem::path elf = temp.path() / "slus_204.97";
    const std::string cnf_text =
        "BOOT2 = cdrom0:\\SLUS_204.97;1\r\n"
        "VER = 1.00\r\n"
        "VMODE = NTSC\r\n";
    write_text(cnf, cnf_text);

    std::vector<std::uint8_t> elf_bytes(
        devices::armax::disc_hash_elf_prefix_size + 64U);
    for (std::size_t index = 0U; index < elf_bytes.size(); ++index) {
        elf_bytes[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
    }
    write_bytes(elf, elf_bytes);

    const auto result = devices::armax::compute_disc_hash_from_files(cnf);
    std::error_code equivalent_error;
    const bool same_file = std::filesystem::equivalent(
        result.elf_path, elf, equivalent_error);
    require(!equivalent_error && same_file,
            "SYSTEM.CNF did not auto-resolve the root ELF case-insensitively");

    const std::vector<std::uint8_t> cnf_bytes(cnf_text.begin(), cnf_text.end());
    const std::vector<std::uint8_t> prefix(
        elf_bytes.begin(),
        elf_bytes.begin() +
            static_cast<std::ptrdiff_t>(devices::armax::disc_hash_elf_prefix_size));
    require(result.hash == devices::armax::compute_disc_hash(cnf_bytes, prefix),
            "File-based AR MAX disc hash differs from the shared CRC formula");

    for (std::size_t index = devices::armax::disc_hash_elf_prefix_size;
         index < elf_bytes.size(); ++index) {
        elf_bytes[index] ^= 0xFFU;
    }
    write_bytes(elf, elf_bytes);
    const auto changed_tail = devices::armax::compute_disc_hash_from_files(cnf);
    require(changed_tail.hash == result.hash,
            "AR MAX disc hash read beyond the first 256 KiB of the ELF");
}

void test_manual_elf_override() {
    TempDirectory temp;
    const std::filesystem::path cnf = temp.path() / "SYSTEM.CNF";
    const std::filesystem::path elsewhere = temp.path() / "DifferentName.bin";
    write_text(cnf, "BOOT2 = cdrom0:\\MISSING.ELF;1\n");
    write_bytes(elsewhere, {1U, 2U, 3U, 4U, 5U});

    bool failed_without_override = false;
    try {
        (void)devices::armax::compute_disc_hash_from_files(cnf);
    } catch (...) {
        failed_without_override = true;
    }
    require(failed_without_override,
            "Missing BOOT2 ELF should require the manual ELF override");

    const auto result = devices::armax::compute_disc_hash_from_files(cnf, elsewhere);
    require(result.elf_path == elsewhere && result.hash != 0U,
            "Manual AR MAX ELF override was not used");
}

void test_mapping_load_save_update() {
    TempDirectory temp;
    const std::filesystem::path mapping = temp.path() / "ArmaxDiscHash.json";
    devices::armax::add_or_update_disc_hash_entry(
        mapping, 0x12345678U, "Example Game", "SLUS_204.97");
    devices::armax::add_or_update_disc_hash_entry(
        mapping, 0x12345678U, "Updated Game", "slus_204.97");
    devices::armax::add_or_update_disc_hash_entry(
        mapping, 0xAABBCCDDU, "Second Game", "SLES_500.00");

    const auto entries = devices::armax::get_disc_hash_entries(mapping);
    require(entries.size() == 2U,
            "AR MAX disc-hash mapping update created a duplicate row");
    require(entries[0].game_name == "Second Game" ||
                entries[0].game_name == "Updated Game",
            "AR MAX disc-hash entries did not load");
    const auto known = devices::armax::try_get_disc_hash_game_name(
        mapping, 0x12345678U);
    require(known && *known == "Updated Game",
            "AR MAX disc-hash game lookup failed");

    std::ifstream input(mapping);
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    require(text.find("SLUS_204.97>12345678>Updated Game\n") != std::string::npos,
            "AR MAX disc-hash mapping did not preserve ELF/hash/name fields");
}

} // namespace

void run_armax_disc_hash_tests() {
    test_system_cnf_boot_parser();
    test_same_root_auto_elf_and_256k_limit();
    test_manual_elf_override();
    test_mapping_load_save_update();
}

} // namespace omni::tests
