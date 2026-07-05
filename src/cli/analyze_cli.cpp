#include "cli/analyze_cli.hpp"

#include "omni/identity.hpp"

#include "analysis/e001_fixer.hpp"
#include "analysis/raw_plausibility.hpp"
#include "convert/conversion_service.hpp"
#include "core/code_format.hpp"
#include "core/inline_crypt.hpp"
#include "devices/action_replay/ar_crypto.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "formats/text/text_cheat_writer.hpp"
#include "translate/transpose_mode.hpp"
#include "util/hex.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace omni::cli {
namespace {

enum class Mode { none, analyze, analyze_fix };

struct Options {
    Mode mode{Mode::none};
    bool manual{};
    bool manual_all{};
    bool fix_e001{};
    Crypt output_crypt{Crypt::raw};
    std::filesystem::path input;
    std::filesystem::path output_root{std::filesystem::current_path()};
};

struct Attempt {
    bool success{};
    bool converted{};
    Crypt input_crypt{Crypt::raw};
    formats::text::CheatBlock block;
    analysis::PlausibilityResult plausibility;
    std::string error;
    std::uint32_t active_ar2_seed{devices::action_replay::ar1_seed};
    bool fixed{};
};

constexpr std::array<Crypt, 12> candidate_menu{{
    Crypt::gameshark3, Crypt::ar1, Crypt::ar2, Crypt::armax,
    Crypt::codebreaker, Crypt::codebreaker7_common, Crypt::gameshark5,
    Crypt::max_raw, Crypt::raw, Crypt::ar2_raw,
    Crypt::codebreaker_raw, Crypt::gameshark_raw
}};

constexpr std::array<std::uint32_t, 3> common_ar2_display_keys{{
    0x1746EAADU, 0x1645EBB3U, 0x1853E59EU
}};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open input file: " + path.string());
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

void write_file(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Unable to create output file: " + path.string());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::optional<Crypt> parse_output_crypt(std::string_view value) {
    return parse_crypt(value);
}

CodeFormat format_for_crypt(Crypt crypt) {
    switch (crypt) {
        case Crypt::ar1: return CodeFormat::action_replay1;
        case Crypt::ar2: return CodeFormat::action_replay2;
        case Crypt::armax: return CodeFormat::action_replay_max;
        case Crypt::codebreaker: return CodeFormat::codebreaker1_6;
        case Crypt::codebreaker7_common: return CodeFormat::codebreaker7_common;
        case Crypt::gameshark3: return CodeFormat::gameshark_madcatz34;
        case Crypt::gameshark5: return CodeFormat::gameshark_madcatz5;
        case Crypt::max_raw: return CodeFormat::armax_raw;
        case Crypt::raw: return CodeFormat::standard_raw;
        case Crypt::ar2_raw: return CodeFormat::ar12_raw;
        case Crypt::codebreaker_raw: return CodeFormat::codebreaker_raw;
        case Crypt::gameshark_raw: return CodeFormat::gameshark3_raw;
    }
    return CodeFormat::standard_raw;
}

std::uint32_t seed_from_displayed_ar2_key(std::uint32_t key) noexcept {
    return devices::action_replay::decrypt_word(key, 0x05U, 0x18U);
}

Crypt declared_crypt(const formats::text::CheatBlock& block) {
    return crypt_from_label(block.label.value_or("")).value_or(Crypt::raw);
}

std::string short_label(const formats::text::CheatBlock& block) {
    std::string label = block.label.value_or("(no label)");
    if (label.size() > 80U) label = label.substr(0U, 77U) + "...";
    return label;
}

std::string serialize_source_block(const formats::text::CheatBlock& block,
                                   Crypt crypt) {
    return formats::text::write_text({block}, crypt);
}

Attempt attempt_convert(const formats::text::CheatBlock& source,
                        Crypt input_crypt, Crypt output_crypt,
                        std::uint32_t ar2_seed, Mode mode,
                        bool force_accept = false) {
    Attempt attempt;
    attempt.input_crypt = input_crypt;
    attempt.active_ar2_seed = ar2_seed;
    try {
        convert::Request request;
        request.input_format = format_for_crypt(input_crypt);
        request.output_format = format_for_crypt(output_crypt);
        request.ar2_key = ar2_seed;
        request.transpose_mode = translate::TransposeMode::original;
        const convert::Result converted = convert::convert_text(
            serialize_source_block(source, input_crypt), request);
        if (converted.blocks.empty()) {
            attempt.error = "conversion returned no cheat block";
            return attempt;
        }
        attempt.block = converted.blocks.front();
        attempt.converted = true;
        if (converted.active_ar2_seed) attempt.active_ar2_seed = *converted.active_ar2_seed;
        if (analysis::is_raw_family(output_crypt) && mode != Mode::none) {
            attempt.plausibility = analysis::validate_raw_plausibility(
                attempt.block.cheat, output_crypt,
                mode == Mode::analyze_fix ? analysis::AnalyzeMode::analyze_fix
                                          : analysis::AnalyzeMode::analyze);
            if (!attempt.plausibility.valid && !force_accept) {
                attempt.error = attempt.plausibility.reason;
                return attempt;
            }
        } else {
            attempt.plausibility = {true, 0U, 0U, "NotAnalyzed", "", ""};
        }
        attempt.success = true;
    } catch (const std::exception& error) {
        attempt.error = error.what();
    }
    return attempt;
}

std::vector<Crypt> candidate_list(Crypt declared) {
    std::vector<Crypt> result{declared};
    for (Crypt candidate : candidate_menu) {
        if (candidate != declared) result.push_back(candidate);
    }
    const auto move_to_second = [&](Crypt value) {
        const auto found = std::find(result.begin(), result.end(), value);
        if (found != result.end() && found != result.begin() && found != result.begin() + 1) {
            result.erase(found);
            result.insert(result.begin() + 1, value);
        }
    };
    if (declared == Crypt::codebreaker7_common) move_to_second(Crypt::codebreaker);
    else if (declared == Crypt::codebreaker) move_to_second(Crypt::codebreaker7_common);
    if (declared == Crypt::ar1) move_to_second(Crypt::ar2);
    else if (declared == Crypt::ar2) move_to_second(Crypt::ar1);
    return result;
}

void print_candidate_menu() {
    for (std::size_t index = 0U; index < candidate_menu.size(); ++index) {
        std::cout << "  " << (index + 1U) << ") " << crypt_token(candidate_menu[index]) << '\n';
    }
}

struct ManualChoice {
    std::optional<Crypt> crypt;
    std::optional<std::uint32_t> ar2_seed;
    bool force{};
    bool skip{};
    bool erase{};
    bool quit{};
};

ManualChoice prompt_manual(const formats::text::CheatBlock& block,
                           Crypt declared, bool failure_only,
                           std::string_view error = {}) {
    std::cout << "\n" << (failure_only ? "Conversion/analyze failure" : "Manual crypt check")
              << ": " << short_label(block) << "\nDeclared: " << crypt_token(declared) << '\n';
    if (!error.empty()) std::cout << "Reason: " << error << '\n';
    print_candidate_menu();
    std::cout << "Enter number, S=skip, D=delete, Q=quit. Optional: -y force, -k<AR2KEY>: ";
    std::string line;
    if (!std::getline(std::cin, line)) return {};
    std::istringstream in(line);
    std::string first;
    in >> first;
    ManualChoice choice;
    const std::string command = upper(first);
    if (command == "S") { choice.skip = true; return choice; }
    if (command == "D") { choice.erase = true; return choice; }
    if (command == "Q") { choice.quit = true; return choice; }
    try {
        const int selected = std::stoi(first);
        if (selected >= 1 && selected <= static_cast<int>(candidate_menu.size())) {
            choice.crypt = candidate_menu[static_cast<std::size_t>(selected - 1)];
        }
    } catch (...) {
        return choice;
    }
    std::string token;
    while (in >> token) {
        const std::string normalized = upper(token);
        if (normalized == "-Y" || normalized == "Y") choice.force = true;
        if (normalized.rfind("-K", 0U) == 0U) {
            std::string hex = token.size() > 2U ? token.substr(2U) : std::string{};
            if (hex.empty()) in >> hex;
            if (const auto key = hex::parse_u32(hex)) choice.ar2_seed = seed_from_displayed_ar2_key(*key);
        }
    }
    return choice;
}

Attempt convert_with_auto_fix(const formats::text::CheatBlock& source,
                              Crypt output_crypt, std::uint32_t ar2_seed,
                              const Options& options, bool& deleted,
                              bool& quit) {
    const Crypt declared = declared_crypt(source);

    // Match OmniConvertCS: declared RAW/CRAW/GRAW inputs are not auto-guessed
    // unless full manual-check mode was requested.
    if (!options.manual_all &&
        (declared == Crypt::raw || declared == Crypt::codebreaker_raw ||
         declared == Crypt::gameshark_raw)) {
        return attempt_convert(source, declared, output_crypt, ar2_seed,
                               options.mode);
    }

    std::vector<Crypt> candidates = candidate_list(declared);
    bool force = false;
    std::optional<std::uint32_t> manual_seed;

    if (options.manual_all) {
        const ManualChoice choice = prompt_manual(source, declared, false);
        deleted = choice.erase;
        quit = choice.quit;
        if (choice.skip || deleted || quit) return {};
        if (choice.crypt) {
            candidates.erase(std::remove(candidates.begin(), candidates.end(), *choice.crypt), candidates.end());
            candidates.insert(candidates.begin(), *choice.crypt);
        }
        force = choice.force;
        manual_seed = choice.ar2_seed;
    }

    Attempt declared_fallback;
    bool have_fallback = false;
    for (std::size_t index = 0U; index < candidates.size(); ++index) {
        const Crypt candidate = candidates[index];
        std::vector<std::uint32_t> seeds{ar2_seed};
        if (candidate == Crypt::ar2) {
            if (manual_seed) seeds = {*manual_seed};
            else {
                for (std::uint32_t key : common_ar2_display_keys) {
                    const std::uint32_t seed = seed_from_displayed_ar2_key(key);
                    if (std::find(seeds.begin(), seeds.end(), seed) == seeds.end()) seeds.push_back(seed);
                }
            }
        }
        for (std::uint32_t seed : seeds) {
            Attempt attempt = attempt_convert(source, candidate, output_crypt, seed,
                                              options.mode, force);
            if (candidate == declared && !have_fallback) {
                declared_fallback = attempt;
                have_fallback = true;
            }
            if (attempt.success) {
                attempt.fixed = candidate != declared;
                return attempt;
            }
        }
    }

    if (options.manual && !options.manual_all) {
        const ManualChoice choice = prompt_manual(source, declared, true,
                                                  have_fallback ? declared_fallback.error : "all candidates failed");
        deleted = choice.erase;
        quit = choice.quit;
        if (choice.crypt && !deleted && !quit && !choice.skip) {
            Attempt attempt = attempt_convert(source, *choice.crypt, output_crypt,
                                              choice.ar2_seed.value_or(ar2_seed),
                                              options.mode, choice.force);
            attempt.fixed = *choice.crypt != declared;
            return attempt;
        }
    }

    if (have_fallback) return declared_fallback;
    return {};
}

bool parse_options(int argc, char** argv, Options& options, std::string& error) {
    int index = 1;
    while (index < argc && argv[index][0] == '-') {
        const std::string arg = upper(argv[index]);
        if (arg == "-A") options.mode = Mode::analyze;
        else if (arg == "-AF") options.mode = Mode::analyze_fix;
        else if (arg == "-M" || arg == "--MANUAL") options.manual = true;
        else if (arg == "-MC" || arg == "--MANUAL-ALL" || arg == "--MANUAL-CHECK-ALL") {
            options.manual = true;
            options.manual_all = true;
        } else if (arg == "-E") options.fix_e001 = true;
        else if (arg == "-O" || arg == "--OUT") {
            if (++index >= argc) { error = "Missing value for -o/--out"; return false; }
            options.output_root = argv[index];
        } else if (arg == "-H" || arg == "--HELP" || arg == "/?") {
            error = "help";
            return false;
        } else {
            error = "Unknown option: " + std::string(argv[index]);
            return false;
        }
        ++index;
    }
    if (index >= argc) { error = "Missing <Output-Crypt>"; return false; }
    const auto crypt = parse_output_crypt(argv[index++]);
    if (!crypt) { error = "Unknown output crypt"; return false; }
    options.output_crypt = *crypt;
    if (index >= argc) { error = "Missing <input.txt | input_folder>"; return false; }
    options.input = argv[index];
    if (!std::filesystem::exists(options.input)) {
        error = "Input path not found: " + options.input.string();
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> gather_files(const Options& options,
                                                std::filesystem::path& input_root) {
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_regular_file(options.input)) {
        input_root = std::filesystem::absolute(options.input).parent_path();
        files.push_back(std::filesystem::absolute(options.input));
        return files;
    }
    input_root = std::filesystem::absolute(options.input);
    const std::filesystem::path output_dir = std::filesystem::absolute(
        options.output_root / std::string(crypt_token(options.output_crypt)));
    for (const auto& entry : std::filesystem::recursive_directory_iterator(options.input)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = upper(entry.path().extension().string());
        if (extension != ".TXT") continue;
        const std::filesystem::path absolute = std::filesystem::absolute(entry.path());
        const std::string a = absolute.string();
        const std::string o = output_dir.string();
        if (a.size() > o.size() && a.compare(0U, o.size(), o) == 0) continue;
        files.push_back(absolute);
    }
    std::sort(files.begin(), files.end());
    return files;
}

int process_file(const Options& options, const std::filesystem::path& input_root,
                 const std::filesystem::path& input_path, int& fixed_total) {
    const std::string source_text = read_file(input_path);
    const std::vector<formats::text::CheatBlock> source_blocks =
        formats::text::parse_inline_text(source_text);
    std::vector<formats::text::CheatBlock> output_blocks;
    std::ostringstream report;
    int failures = 0;
    int fixed = 0;
    std::uint32_t ar2_seed = devices::action_replay::ar1_seed;

    for (const auto& source : source_blocks) {
        if (source.cheat.empty()) {
            output_blocks.push_back(source);
            continue;
        }
        const Crypt declared = declared_crypt(source);
        Attempt attempt;
        bool deleted = false;
        bool quit = false;
        if (options.mode == Mode::analyze_fix) {
            attempt = convert_with_auto_fix(source, options.output_crypt, ar2_seed,
                                            options, deleted, quit);
            if (quit) throw std::runtime_error("Manual mode: quit requested");
            if (deleted) {
                report << "DELETED: " << short_label(source) << '\n';
                continue;
            }
        } else {
            attempt = attempt_convert(source, declared, options.output_crypt,
                                      ar2_seed, options.mode);
        }

        if (attempt.success) {
            output_blocks.push_back(attempt.block);
            if (attempt.input_crypt == Crypt::ar2) ar2_seed = attempt.active_ar2_seed;
            if (attempt.fixed) {
                ++fixed;
                report << "FIXED: " << short_label(source) << " declared->used: "
                       << crypt_token(declared) << " -> " << crypt_token(attempt.input_crypt) << '\n';
            }
        } else {
            ++failures;
            if (attempt.converted) {
                // OmniConvertCS keeps the converted/fallback code in the output
                // even when its RAW-family plausibility check fails.
                output_blocks.push_back(attempt.block);
            } else {
                formats::text::CheatBlock error_block = source;
                error_block.cheat.words.clear();
                error_block.conversion_error =
                    "Error code: analyze/convert\nDeclared input: " + std::string(crypt_token(declared)) +
                    "\nDetails: " + (attempt.error.empty() ? "all candidate formats failed" : attempt.error);
                output_blocks.push_back(std::move(error_block));
            }
            report << "FAIL: " << short_label(source) << " in=" << crypt_token(declared);
            if (!attempt.plausibility.rule.empty()) {
                report << " addr28=0x" << hex::format_u32(attempt.plausibility.checked_addr28).substr(1U)
                       << " rule=" << attempt.plausibility.rule;
            }
            report << " reason=" << (attempt.error.empty() ? "all candidates failed" : attempt.error) << '\n';
        }
    }

    std::string output_text = formats::text::write_text(output_blocks, options.output_crypt);
    if (options.fix_e001 && options.output_crypt == Crypt::codebreaker_raw) {
        output_text = analysis::rewrite_e001_to_d(output_text);
    }

    const std::filesystem::path relative = std::filesystem::is_directory(options.input)
        ? std::filesystem::relative(input_path, input_root)
        : input_path.filename();
    const std::filesystem::path output_dir =
        options.output_root / std::string(crypt_token(options.output_crypt));
    const std::filesystem::path output_path = output_dir / relative;
    write_file(output_path, output_text);

    if (options.mode != Mode::none) {
        std::filesystem::path analysis_relative = relative;
        analysis_relative.replace_extension(".analysis.txt");
        std::ostringstream text;
        text << "Input: " << input_path.string() << '\n'
             << "Output: " << output_path.string() << '\n'
             << "OutputCrypt: " << crypt_token(options.output_crypt) << '\n'
             << "Mode: " << (options.mode == Mode::analyze_fix ? "AnalyzeFix" : "Analyze") << '\n'
             << "Failures: " << failures << '\n'
             << "Fixed: " << fixed << "\n\n" << report.str();
        write_file(output_dir / "Analysis" / analysis_relative, text.str());
    }

    fixed_total += fixed;
    std::cout << relative.string() << " -> " << crypt_token(options.output_crypt)
              << " (fail=" << failures << ", fixed=" << fixed << ")\n";
    return failures;
}

} // namespace

void print_analyze_usage() {
    std::cout
        << "C#-compatible batch analyze/auto-fix:\n"
        << "  " << omni::identity::executable_name
        << " [-a|-af] [-m|-mc] [-e] [-o folder] <Output-Crypt> <input.txt|input_folder>\n"
        << "  -a   analyze RAW-family output\n"
        << "  -af  analyze and try alternate input crypts per cheat\n"
        << "  -m   prompt when automatic attempts fail\n"
        << "  -mc  manually choose/review every cheat\n"
        << "  -e   CRAW only: rewrite E001vvvv 0aaaaaaa to Daaaaaaa 0000vvvv\n";
}

std::optional<int> try_run_analyze_cli(int argc, char** argv) {
    if (argc < 2) return std::nullopt;
    const std::string first = argv[1];
    const bool looks_like = (!first.empty() && first[0] == '-') || parse_output_crypt(first).has_value();
    if (!looks_like) return std::nullopt;

    Options options;
    std::string error;
    if (!parse_options(argc, argv, options, error)) {
        if (error == "help") {
            print_analyze_usage();
            return 0;
        }
        std::cerr << error << '\n';
        print_analyze_usage();
        return 1;
    }

    std::filesystem::path input_root;
    const auto files = gather_files(options, input_root);
    if (files.empty()) {
        std::cerr << "No .txt files found to process.\n";
        return 1;
    }

    int failed_files = 0;
    int fixed_total = 0;
    for (const auto& path : files) {
        try {
            if (process_file(options, input_root, path, fixed_total) > 0) ++failed_files;
        } catch (const std::exception& exception) {
            ++failed_files;
            std::cerr << "ERROR: " << path.string() << " " << exception.what() << '\n';
        }
    }
    if (options.mode == Mode::analyze_fix && failed_files > 0 &&
        analysis::is_raw_family(options.output_crypt)) return 3;
    if (options.mode == Mode::analyze && failed_files > 0 &&
        analysis::is_raw_family(options.output_crypt)) return 2;
    return 0;
}

} // namespace omni::cli
