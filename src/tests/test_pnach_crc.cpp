#include "tests/test_pnach_crc.hpp"

#include "formats/pnach/pnach_crc.hpp"
#include "tests/test_support.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace omni::tests {
namespace {

class TempDirectory {
public:
    TempDirectory() {
        const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("omniconvert-pnach-crc-" + std::to_string(nonce));
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void write_bytes(const std::filesystem::path& path,
                 const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void test_elf_crc_matches_csharp_xor() {
    TempDirectory temp;
    const std::filesystem::path elf = temp.path() / "SLUS_123.45";
    write_bytes(elf, {0x78U, 0x56U, 0x34U, 0x12U,
                      0xEFU, 0xCDU, 0xABU, 0x90U,
                      0xAAU, 0xBBU});
    require(formats::pnach::compute_elf_crc(elf) == (0x12345678U ^ 0x90ABCDEFU),
            "PNACH ELF CRC did not XOR complete little-endian words");
}

void test_elf_name_normalization() {
    require(formats::pnach::normalize_elf_name("C:/games/SLUS_123.45") == "SLUS-12345",
            "Standard PS2 ELF name was not normalized");
    require(formats::pnach::normalize_elf_name("sles_526.41") == "SLES-52641",
            "Lowercase PS2 ELF name was not normalized");
    require(formats::pnach::normalize_elf_name("custom.elf") == "custom.elf",
            "Nonstandard ELF name should be preserved");
}

void test_mapping_load_sort_and_lookup() {
    TempDirectory temp;
    const std::filesystem::path mapping = temp.path() / "PnachCRC.json";
    {
        std::ofstream output(mapping);
        output << "// comment\n"
               << "SLES-52641>11223344>Zeta Game\n"
               << "AABBCCDD>Alpha Game\n"
               << "SLUS-12345>11223344>Beta Game\n"
               << "invalid line\n";
    }

    const auto entries = formats::pnach::get_crc_entries(mapping);
    require(entries.size() == 3U, "PNACH CRC mapping parser accepted/rejected wrong rows");
    require(entries[0].game_name == "Alpha Game" && entries[1].game_name == "Beta Game",
            "PNACH CRC entries were not sorted by game name");
    const auto game = formats::pnach::try_get_game_name(mapping, 0x11223344U);
    require(game && *game == "Zeta Game",
            "PNACH CRC game lookup did not preserve file-order first match");
    const auto elf = formats::pnach::try_get_elf_name(mapping, 0x11223344U);
    require(elf && *elf == "SLES-52641",
            "PNACH CRC ELF lookup did not return first named match");
}


void test_mapping_old_mac_line_endings() {
    TempDirectory temp;
    const std::filesystem::path mapping = temp.path() / "PnachCRC.json";
    {
        std::ofstream output(mapping, std::ios::binary | std::ios::trunc);
        output << "SLUS-12345>12345678>Example Game\r"
               << "AABBCCDD>Second Game\r";
    }

    const auto entries = formats::pnach::load_crc_entries(mapping);
    require(entries.size() == 2U,
            "standalone CR PNACH CRC mapping collapsed into one line");
}

void test_add_or_update_and_backward_compatible_save() {
    TempDirectory temp;
    const std::filesystem::path mapping = temp.path() / "PnachCRC.json";
    formats::pnach::add_or_update_crc_entry(mapping, 0x12345678U,
                                             "Example Game", "SLUS_123.45");
    formats::pnach::add_or_update_crc_entry(mapping, 0x12345678U,
                                             "Updated Game", "slus-12345");
    formats::pnach::add_or_update_crc_entry(mapping, 0xAABBCCDDU,
                                             "No ELF Game", "");

    const auto entries = formats::pnach::load_crc_entries(mapping);
    require(entries.size() == 2U, "PNACH CRC update created a duplicate CRC/ELF row");

    std::ifstream input(mapping);
    const std::string text((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    require(text.find("AABBCCDD>No ELF Game\n") != std::string::npos,
            "Backward-compatible CRC>GameName row was not saved");
    require(text.find("SLUS-12345>12345678>Updated Game\n") != std::string::npos,
            "ELFName>CRC>GameName row was not normalized or updated");
}

} // namespace

void run_pnach_crc_tests() {
    test_elf_crc_matches_csharp_xor();
    test_elf_name_normalization();
    test_mapping_load_sort_and_lookup();
    test_mapping_old_mac_line_endings();
    test_add_or_update_and_backward_compatible_save();
}

} // namespace omni::tests
