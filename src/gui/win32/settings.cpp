#include "gui/win32/settings.hpp"

#include "core/code_format.hpp"
#include "translate/transpose_mode.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace omni::gui::win32 {
namespace {

std::wstring settings_path() {
    std::wstring path(32768U, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0U || length >= path.size()) return L"OmniconvertSettings.json";
    path.resize(length);
    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L"OmniconvertSettings.json";
    path.resize(slash + 1U);
    path += L"OmniconvertSettings.json";
    return path;
}

std::optional<std::string> json_string(const std::string& json,
                                       const char* key) {
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(json, match, expression)) return std::nullopt;
    return match[1].str();
}

std::optional<std::uint32_t> json_uint(const std::string& json,
                                       const char* key) {
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, expression)) return std::nullopt;
    try {
        return static_cast<std::uint32_t>(std::stoul(match[1].str()));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> json_bool(const std::string& json, const char* key) {
    const std::regex expression(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*(true|false)",
                                std::regex::icase);
    std::smatch match;
    if (!std::regex_search(json, match, expression)) return std::nullopt;
    std::string value = match[1].str();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value == "true";
}

std::string escape_json(std::string value) {
    std::string output;
    output.reserve(value.size());
    for (const char c : value) {
        if (c == '\\' || c == '"') output.push_back('\\');
        output.push_back(c);
    }
    return output;
}

} // namespace

void load_app_settings(AppState& state) noexcept {
    try {
        std::ifstream input(std::filesystem::path(settings_path()), std::ios::binary);
        if (!input) return;
        const std::string json((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());

        if (const auto value = json_string(json, "InputFormat")) {
            if (const auto format = parse_code_format(*value)) state.input_format = *format;
        }
        if (const auto value = json_string(json, "OutputFormat")) {
            if (const auto format = parse_code_format(*value);
                format && *format != CodeFormat::inline_headers) {
                state.output_format = *format;
            }
        }
        if (const auto value = json_string(json, "VerifierMode")) {
            std::string mode = *value;
            std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            state.armax_verifier_mode = mode == "MANUAL"
                                            ? convert::ArmaxVerifierMode::manual
                                            : convert::ArmaxVerifierMode::automatic;
        }
        if (const auto value = json_uint(json, "Region"); value && *value <= 2U) {
            state.armax_region = static_cast<std::uint8_t>(*value);
        }
        if (const auto value = json_bool(json, "MakeFolders")) {
            state.make_organizers = *value;
        }
        if (const auto value = json_string(json, "HashDrive")) {
            state.armax_hash_drive = value->empty()
                                         ? '\0'
                                         : static_cast<char>(std::toupper(
                                               static_cast<unsigned char>((*value)[0])));
        }
        if (const auto value = json_string(json, "HashSource")) {
            std::string source = *value;
            std::transform(source.begin(), source.end(), source.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            state.armax_disc_hash_source = source == "MANUAL"
                ? ArmaxDiscHashSource::manual
                : source == "DRIVE" ? ArmaxDiscHashSource::drive
                                     : ArmaxDiscHashSource::none;
        } else if (state.armax_hash_drive != '\0') {
            state.armax_disc_hash_source = ArmaxDiscHashSource::drive;
        }
        if (const auto value = json_uint(json, "ManualDiscHash"); value && *value != 0U) {
            state.armax_manual_disc_hash = *value;
        }
        if (const auto value = json_string(json, "DiscHashGameName")) {
            state.armax_disc_hash_game_name = *value;
        }
        if (const auto value = json_string(json, "DiscHashElfName")) {
            state.armax_disc_hash_elf_name = *value;
        }
        if (state.armax_disc_hash_source == ArmaxDiscHashSource::manual &&
            !state.armax_manual_disc_hash) {
            state.armax_disc_hash_source = ArmaxDiscHashSource::none;
        }
        if (const auto value = json_uint(json, "Gs3Key"); value && *value <= 4U) {
            state.gs3_key = static_cast<std::uint8_t>(*value);
        }
        if (const auto value = json_uint(json, "CbcSaveVersion")) {
            state.cbc_version = *value == 8U
                                    ? formats::binary::CbcVersion::v8
                                    : formats::binary::CbcVersion::v7;
        }
        if (const auto value = json_uint(json, "GameId"); value && *value <= 0xFFFFU) {
            state.armax_game_id = *value == 0U ? 0x0357U : *value;
        }
        if (const auto value = json_string(json, "TransposeType")) {
            std::string mode = *value;
            std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            state.transpose_mode = mode == "STRICT"
                                       ? translate::TransposeMode::strict
                                       : translate::TransposeMode::original;
        }
        if (const auto value = json_bool(json, "CmpOutputMode")) {
            state.cmp_output_mode = *value;
        }
    } catch (...) {
        // Match the C# application: corrupt or inaccessible settings are ignored.
    }
}

void save_app_settings(const AppState& state) noexcept {
    try {
        std::ofstream output(std::filesystem::path(settings_path()),
                             std::ios::binary | std::ios::trunc);
        if (!output) return;
        const std::string input_format =
            escape_json(std::string(code_format_token(state.input_format)));
        const std::string output_format =
            escape_json(std::string(code_format_token(state.output_format)));
        const char hash_drive = state.armax_hash_drive;
        const std::string hash_source =
            state.armax_disc_hash_source == ArmaxDiscHashSource::manual
                ? "MANUAL"
                : state.armax_disc_hash_source == ArmaxDiscHashSource::drive
                      ? "DRIVE" : "NONE";
        const std::string disc_hash_game_name =
            escape_json(state.armax_disc_hash_game_name);
        const std::string disc_hash_elf_name =
            escape_json(state.armax_disc_hash_elf_name);
        output << "{\n"
               << "  \"InputFormat\": \"" << input_format << "\",\n"
               << "  \"OutputFormat\": \"" << output_format << "\",\n"
               << "  \"VerifierMode\": \""
               << (state.armax_verifier_mode == convert::ArmaxVerifierMode::manual
                       ? "MANUAL" : "AUTO") << "\",\n"
               << "  \"Region\": " << static_cast<unsigned>(state.armax_region) << ",\n"
               << "  \"MakeFolders\": "
               << (state.make_organizers ? "true" : "false") << ",\n"
               << "  \"HashDrive\": \""
               << (hash_drive == '\0' ? std::string() : std::string(1, hash_drive))
               << "\",\n"
               << "  \"HashSource\": \"" << hash_source << "\",\n"
               << "  \"ManualDiscHash\": "
               << state.armax_manual_disc_hash.value_or(0U) << ",\n"
               << "  \"DiscHashGameName\": \""
               << disc_hash_game_name << "\",\n"
               << "  \"DiscHashElfName\": \""
               << disc_hash_elf_name << "\",\n"
               << "  \"Gs3Key\": " << static_cast<unsigned>(state.gs3_key) << ",\n"
               << "  \"CbcSaveVersion\": "
               << (state.cbc_version == formats::binary::CbcVersion::v8 ? 8 : 7)
               << ",\n"
               << "  \"GameId\": " << state.armax_game_id << ",\n"
               << "  \"TransposeType\": \""
               << (state.transpose_mode == translate::TransposeMode::original
                       ? "ORIGINAL" : "STRICT") << "\",\n"
               << "  \"CmpOutputMode\": "
               << (state.cmp_output_mode ? "true" : "false") << "\n"
               << "}\n";
    } catch (...) {
        // Best effort only, matching the C# implementation.
    }
}

} // namespace omni::gui::win32
