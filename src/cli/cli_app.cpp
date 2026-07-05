#include "cli/cli_app.hpp"

#include "omni/identity.hpp"

#include "convert/conversion_service.hpp"
#include "cli/analyze_cli.hpp"
#include "core/code_format.hpp"
#include "formats/mips/mips_r5900_text.hpp"
#include "formats/binary/application_formats.hpp"
#include "formats/pnach/pnach_writer.hpp"
#include "formats/pnach/pnach_crc.hpp"
#include "devices/armax/armax_disc_hash.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "formats/text/text_cheat_writer.hpp"
#include "util/hex.hpp"
#include "translate/transpose_mode.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <optional>
#include <algorithm>
#include <cctype>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open input file: " + path);

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_result(const std::string& text, const char* output_path) {
    if (output_path == nullptr) {
        std::cout << text;
        return;
    }

    std::ofstream output(output_path, std::ios::binary);
    if (!output) throw std::runtime_error(std::string("Unable to open output file: ") + output_path);
    output << text;
}

void print_usage() {
    const std::string_view executable = omni::identity::executable_name;
    std::cout
        << omni::identity::window_title << "\n"
        << "\n"
        << "Usage:\n"
        << "  " << executable << " [-a|-af] [-m|-mc] [-e] [-o folder] <Output-Crypt> <input.txt|input_folder>\n"
        << "  " << executable << " normalize <input.txt> [output.txt] [output-format]\n"
        << "  " << executable << " convert <input-format> <output-format> <input.txt> [output.txt] [ar2-key] [gs-key] [armax-key] [armax-game-id] [armax-region] [transpose-mode] [pnach-crc] [pnach-game-name]\n"
        << "  " << executable << " pnach-crc <ps2-elf> [game-name] [mapping-file]\n"
        << "  " << executable << " armax-disc-hash <SYSTEM.CNF> [--elf path] [--game name] [--mapping file]\n"
        << "  " << executable << " binary-import <ARMAXBIN|CBC|P2M|SMBIN> <input-file> [output.txt]\n"
        << "  " << executable << " binary-export <ARMAXBIN|CBC7|CBC8|P2M|SMBIN> <input-format> <input.txt> <output-file> [game-name] [game-id] [transpose-mode]\n"
        << "\n"
        << "Encrypted formats:\n"
        << "  AR1, AR2, ARMAX, CB, CB7_COMMON, GS1, GS2, GS3, GS5, SMC, XP13, XP4, XP5\n"
        << "\n"
        << "Raw formats:\n"
        << "  RAW, INLINE (input only), MIPS, PNACH, MAXRAW, ARAW, CRAW, GS12RAW, GRAW\n"
        << "\n"
        << "Binary application formats:\n"
        << "  ARMAXBIN, CBC/CBC7, CBC8, P2M, SMBIN\n"
        << "\n"
        << "Transpose modes:\n"
        << "  STRICT (default) or ORIGINAL (OmniConvertCS-compatible translation)\n"
        << "\n"
        << "Alias notes:\n"
        << "  GS1 and SMC use AR1 crypto; GS2 uses AR2 crypto.\n"
        << "  XP13 uses CodeBreaker V1-V6 crypto; XP4/XP5 use GS3/GS5 crypto.\n"
        << "\n"
        << "Examples:\n"
        << "  " << executable << " convert XP13 CRAW encrypted.txt decrypted.txt\n"
        << "  " << executable << " convert CRAW XP13 decrypted.txt encrypted.txt\n"
        << "  " << executable << " convert GS1 ARAW encrypted.txt decrypted.txt\n"
        << "  " << executable << " convert ARAW SMC raw.txt encrypted.txt\n"
        << "  " << executable << " convert RAW AR2 raw.txt encrypted.txt 04030209\n"
        << "  " << executable << " convert INLINE PNACH mixed.txt patches.pnach\n"
        << "  " << executable << " convert MIPS RAW cave.asm cave.txt\n"
        << "  " << executable << " convert RAW MIPS raw.txt cave.asm\n"
        << "  " << executable << " convert PNACH CRAW patches.pnach cb-raw.txt\n"
        << "  " << executable << " convert CRAW PNACH cb-raw.txt patches.pnach\n"
        << "  " << executable << " convert CRAW PNACH cb-raw.txt patches.pnach 04030209 0 04030209 0 0 STRICT 12345678 \"Sample Game\"\n"
        << "  " << executable << " pnach-crc SLUS_123.45 \"Sample Game\"\n"
        << "  " << executable << " armax-disc-hash SYSTEM.CNF --game \"Sample Game\"\n"
        << "  " << executable << " binary-import CBC codes.cbc codes.txt\n"
        << "  " << executable << " binary-export ARMAXBIN RAW codes.txt codes.bin \"Sample Game\" 0123 ORIGINAL\n"
        << "  " << executable << " binary-export CBC8 RAW codes.txt codes.cbc \"Sample Game\"\n"
        << "  " << executable << " convert RAW GS5 raw.txt encrypted.txt 04030209 4\n"
        << "  " << executable << " convert MAXRAW ARMAX raw.txt encrypted.txt 04030209 0 04030209\n";
}



std::string uppercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

struct BinarySelection {
    omni::formats::binary::ApplicationFormat format{};
    omni::formats::binary::CbcVersion cbc_version{omni::formats::binary::CbcVersion::v7};
};

std::optional<BinarySelection> parse_binary_selection(std::string token) {
    token = uppercase_ascii(std::move(token));
    using omni::formats::binary::ApplicationFormat;
    using omni::formats::binary::CbcVersion;
    if (token == "ARMAXBIN" || token == "ARMAX_BIN" || token == "MAXBIN") {
        return BinarySelection{ApplicationFormat::armax_bin, CbcVersion::v7};
    }
    if (token == "CBC" || token == "CBC7") {
        return BinarySelection{ApplicationFormat::codebreaker_cbc, CbcVersion::v7};
    }
    if (token == "CBC8") {
        return BinarySelection{ApplicationFormat::codebreaker_cbc, CbcVersion::v8};
    }
    if (token == "P2M" || token == "GSP2M" || token == "XPP2M") {
        return BinarySelection{ApplicationFormat::gameshark_p2m, CbcVersion::v7};
    }
    if (token == "SMBIN" || token == "SWAPMAGICBIN" || token == "SCF") {
        return BinarySelection{ApplicationFormat::swap_magic_bin, CbcVersion::v7};
    }
    return std::nullopt;
}

int run_binary_import(int argc, char** argv) {
    if (argc < 4) {
        print_usage();
        return 1;
    }
    const auto selection = parse_binary_selection(argv[2]);
    if (!selection) {
        std::cerr << "Unknown or unsupported binary input format: " << argv[2] << '\n';
        return 3;
    }
    const omni::formats::binary::LoadResult loaded =
        omni::formats::binary::load_application_file(selection->format, argv[3]);
    const std::string text = omni::formats::text::write_text(
        loaded.blocks, omni::crypt_for_format(loaded.input_format));
    write_result(text, argc >= 5 ? argv[4] : nullptr);
    if (!loaded.game_name.empty()) std::cerr << "GameName: " << loaded.game_name << '\n';
    if (loaded.game_id != 0U) {
        std::cerr << "GameID: " << omni::hex::format_u32(loaded.game_id) << '\n';
    }
    for (const std::string& warning : loaded.warnings) {
        std::cerr << "Warning: " << warning << '\n';
    }
    return 0;
}

int run_binary_export(int argc, char** argv) {
    if (argc < 6) {
        print_usage();
        return 1;
    }
    const auto selection = parse_binary_selection(argv[2]);
    if (!selection) {
        std::cerr << "Unknown binary output format: " << argv[2] << '\n';
        return 3;
    }
    const auto input_format = omni::parse_code_format(argv[3]);
    if (!input_format) {
        std::cerr << "Unknown input format: " << argv[3] << '\n';
        return 3;
    }

    omni::CodeFormat output_format = omni::CodeFormat::standard_raw;
    using omni::formats::binary::ApplicationFormat;
    switch (selection->format) {
        case ApplicationFormat::armax_bin:
            output_format = omni::CodeFormat::action_replay_max;
            break;
        case ApplicationFormat::codebreaker_cbc:
            output_format = selection->cbc_version == omni::formats::binary::CbcVersion::v8
                                ? omni::CodeFormat::codebreaker7_common
                                : omni::CodeFormat::codebreaker1_6;
            break;
        case ApplicationFormat::gameshark_p2m:
            output_format = omni::CodeFormat::gameshark_madcatz5;
            break;
        case ApplicationFormat::swap_magic_bin:
            output_format = omni::CodeFormat::standard_raw;
            break;
    }

    omni::convert::Request request;
    request.input_format = *input_format;
    request.output_format = output_format;
    if (argc >= 7) request.game_name = argv[6];
    if (argc >= 8) {
        const auto game_id = omni::hex::parse_u32(argv[7]);
        if (!game_id || *game_id > 0x1FFFU) {
            std::cerr << "Invalid Game ID; expected hexadecimal 0000 through 1FFF\n";
            return 7;
        }
        request.armax_game_id = *game_id;
    }
    if (argc >= 9) {
        const auto mode = omni::translate::parse_transpose_mode(argv[8]);
        if (!mode) {
            std::cerr << "Invalid transpose mode; expected STRICT or ORIGINAL\n";
            return 9;
        }
        request.transpose_mode = *mode;
    }

    const omni::convert::Result converted =
        omni::convert::convert_text(read_file(argv[4]), request);
    omni::formats::binary::WriteOptions options;
    options.game_name = request.game_name;
    options.game_id = request.armax_game_id;
    options.cbc_version = selection->cbc_version;
    omni::formats::binary::write_application_file(
        selection->format, argv[5], converted.blocks, options);
    for (const std::string& warning : converted.warnings) {
        std::cerr << "Warning: " << warning << '\n';
    }
    return 0;
}

std::filesystem::path executable_directory(const char* argv0) {
    std::error_code error;
    std::filesystem::path executable = std::filesystem::absolute(
        argv0 != nullptr ? std::filesystem::path(argv0) : std::filesystem::path{}, error);
    if (!error && !executable.empty()) return executable.parent_path();
    return std::filesystem::current_path();
}

int run_pnach_crc(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const std::filesystem::path elf_path = argv[2];
    const std::uint32_t crc = omni::formats::pnach::compute_elf_crc(elf_path);
    const std::string elf_name = omni::formats::pnach::normalize_elf_name(
        elf_path.filename().string());
    const std::filesystem::path mapping_path = argc >= 5
        ? std::filesystem::path(argv[4])
        : executable_directory(argv[0]) / "PnachCRC.json";

    std::string game_name;
    if (argc >= 4) {
        game_name = argv[3];
        omni::formats::pnach::add_or_update_crc_entry(
            mapping_path, crc, game_name, elf_name);
    } else if (const auto known = omni::formats::pnach::try_get_game_name(mapping_path, crc)) {
        game_name = *known;
    }

    std::cout << "CRC: " << omni::hex::format_u32(crc) << '\n'
              << "ELF: " << elf_name << '\n';
    if (!game_name.empty()) std::cout << "GameName: " << game_name << '\n';
    if (argc >= 4) std::cout << "Mapping: " << mapping_path.string() << '\n';
    return 0;
}


int run_armax_disc_hash(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const std::filesystem::path system_cnf_path = argv[2];
    std::optional<std::filesystem::path> elf_path;
    std::string game_name;
    std::filesystem::path mapping_path =
        executable_directory(argv[0]) / "ArmaxDiscHash.json";

    for (int index = 3; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--elf" && index + 1 < argc) {
            elf_path = std::filesystem::path(argv[++index]);
        } else if (option == "--game" && index + 1 < argc) {
            game_name = argv[++index];
        } else if (option == "--mapping" && index + 1 < argc) {
            mapping_path = std::filesystem::path(argv[++index]);
        } else {
            std::cerr << "Unknown or incomplete armax-disc-hash option: "
                      << option << '\n';
            return 3;
        }
    }

    const auto result = omni::devices::armax::compute_disc_hash_from_files(
        system_cnf_path, elf_path);
    const std::string elf_name = result.elf_path.filename().string();
    if (!game_name.empty()) {
        omni::devices::armax::add_or_update_disc_hash_entry(
            mapping_path, result.hash, game_name, elf_name);
    } else if (const auto known =
                   omni::devices::armax::try_get_disc_hash_game_name(
                       mapping_path, result.hash)) {
        game_name = *known;
    }

    std::cout << "DiscHash: " << omni::hex::format_u32(result.hash) << '\n'
              << "SYSTEM.CNF: " << result.system_cnf_path.string() << '\n'
              << "BOOT2: " << result.boot_path << '\n'
              << "ELF: " << result.elf_path.string() << '\n';
    if (!game_name.empty()) std::cout << "GameName: " << game_name << '\n';
    if (!game_name.empty()) std::cout << "Mapping: " << mapping_path.string() << '\n';
    return 0;
}

int run_normalize(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const std::string input_path = argv[2];
    const auto blocks = omni::formats::text::parse_file(input_path);

    omni::CodeFormat output_format = omni::CodeFormat::standard_raw;
    if (argc >= 5) {
        const auto parsed = omni::parse_code_format(argv[4]);
        if (!parsed) {
            std::cerr << "Unknown output format: " << argv[4] << '\n';
            return 3;
        }
        output_format = *parsed;
    }

    const std::string normalized = output_format == omni::CodeFormat::pnach_raw
        ? omni::formats::pnach::write_text(blocks)
        : (output_format == omni::CodeFormat::ps2_mips_r5900
               ? omni::formats::mips_r5900::write_text(blocks)
               : omni::formats::text::write_text(blocks, omni::crypt_for_format(output_format)));
    write_result(normalized, argc >= 4 ? argv[3] : nullptr);
    return 0;
}

int run_convert(int argc, char** argv) {
    if (argc < 5) {
        print_usage();
        return 1;
    }

    const auto input_format = omni::parse_code_format(argv[2]);
    const auto output_format = omni::parse_code_format(argv[3]);
    if (!input_format) {
        std::cerr << "Unknown input format: " << argv[2] << '\n';
        return 3;
    }
    if (!output_format) {
        std::cerr << "Unknown output format: " << argv[3] << '\n';
        return 3;
    }

    omni::convert::Request request{*input_format, *output_format,
                                   omni::devices::action_replay::ar1_seed};
    if (argc >= 7) {
        const auto key = omni::hex::parse_u32(argv[6]);
        if (!key || std::string(argv[6]).size() != 8U) {
            std::cerr << "Invalid AR2 key; expected exactly eight hexadecimal digits\n";
            return 4;
        }
        request.ar2_key = *key;
    }
    if (argc >= 8) {
        const auto key = omni::hex::parse_u32(argv[7]);
        if (!key || *key > 7U || std::string(argv[7]).size() != 1U) {
            std::cerr << "Invalid GameShark key; expected one digit from 0 through 7\n";
            return 5;
        }
        request.gs3_key = static_cast<std::uint8_t>(*key);
    }

    if (argc >= 9) {
        const auto key = omni::hex::parse_u32(argv[8]);
        if (!key || std::string(argv[8]).size() != 8U) {
            std::cerr << "Invalid ARMAX payload key; expected exactly eight hexadecimal digits\n";
            return 6;
        }
        request.armax_key = *key;
    }


    if (argc >= 10) {
        const auto game_id = omni::hex::parse_u32(argv[9]);
        if (!game_id || *game_id > 0x1FFFU) {
            std::cerr << "Invalid ARMAX Game ID; expected hexadecimal 0000 through 1FFF\n";
            return 7;
        }
        request.armax_game_id = *game_id;
    }

    if (argc >= 11) {
        const auto region = omni::hex::parse_u32(argv[10]);
        if (!region || *region > 3U) {
            std::cerr << "Invalid ARMAX region; expected 0 through 3\n";
            return 8;
        }
        request.armax_region = static_cast<std::uint8_t>(*region);
    }

    if (argc >= 12) {
        const auto mode = omni::translate::parse_transpose_mode(argv[11]);
        if (!mode) {
            std::cerr << "Invalid transpose mode; expected STRICT or ORIGINAL\n";
            return 9;
        }
        request.transpose_mode = *mode;
    }

    if (argc >= 13) {
        const auto crc = omni::hex::parse_u32(argv[12]);
        if (!crc || std::string(argv[12]).size() != 8U) {
            std::cerr << "Invalid PNACH CRC; expected exactly eight hexadecimal digits\n";
            return 11;
        }
        request.pnach_include_crc = true;
        request.pnach_crc = *crc;
    }

    if (argc >= 14) request.game_name = argv[13];

    const omni::convert::Result result = omni::convert::convert_text(read_file(argv[4]), request);
    write_result(result.output_text, argc >= 6 ? argv[5] : nullptr);

    for (const std::string& warning : result.warnings) {
        std::cerr << "Warning: " << warning << '\n';
    }
    return 0;
}

} // namespace

namespace omni::cli {

int run_cli(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            print_usage();
            omni::cli::print_analyze_usage();
            return argc < 2 ? 1 : 0;
        }

        if (const auto analyzed = omni::cli::try_run_analyze_cli(argc, argv)) {
            return *analyzed;
        }

        const std::string command = argv[1];
        if (command == "normalize") return run_normalize(argc, argv);
        if (command == "convert") return run_convert(argc, argv);
        if (command == "pnach-crc") return run_pnach_crc(argc, argv);
        if (command == "armax-disc-hash") return run_armax_disc_hash(argc, argv);
        if (command == "binary-import") return run_binary_import(argc, argv);
        if (command == "binary-export") return run_binary_export(argc, argv);

        std::cerr << "Unknown command: " << command << '\n';
        print_usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 10;
    }
}

} // namespace omni::cli
