#include "convert/conversion_service.hpp"

#include "core/inline_crypt.hpp"
#include "devices/action_replay/ar_batch.hpp"
#include "devices/armax/armax_crypto.hpp"
#include "devices/armax/armax_options.hpp"
#include "devices/codebreaker/cb_batch.hpp"
#include "devices/gameshark/gs3_crypto.hpp"
#include "devices/gameshark/gs3_verifier.hpp"
#include "formats/mips/mips_r5900_text.hpp"
#include "formats/pnach/pnach_parser.hpp"
#include "formats/pnach/pnach_writer.hpp"
#include "formats/text/text_cheat_parser.hpp"
#include "formats/text/text_cheat_writer.hpp"
#include "translate/translation_error.hpp"
#include "translate/translator.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace omni::convert {
namespace {

using formats::text::CheatBlock;

std::optional<devices::armax::Metadata> decrypt_block(
    CheatBlock& block, Crypt crypt, std::uint32_t armax_key,
    devices::action_replay::Context& ar2_context,
    bool ar2_raw_enable_input) {
    using namespace devices::action_replay;

    switch (crypt) {
        case Crypt::ar1:
            decrypt_ar1(block.cheat.words);
            break;
        case Crypt::ar2:
            if (block.cheat.word_count() >= 2U &&
                block.cheat.words[0] == ar2_raw_enable_address) {
                const std::uint32_t displayed_key = block.cheat.words[1];
                block.cheat.words.erase(block.cheat.words.begin(),
                                        block.cheat.words.begin() + 2);

                if (displayed_key != ar2_raw_enable_value) {
                    ar2_context.set_seed(seed_from_displayed_key(displayed_key));
                    ar2_context.decrypt_ar2(block.cheat.words, true);
                }
            } else if (ar2_raw_enable_input) {
                remove_ar2_raw_enable_code(block.cheat.words);
            } else {
                ar2_context.decrypt_ar2(block.cheat.words, true);
            }
            break;
        case Crypt::armax:
            return devices::armax::decrypt_full(block.cheat.words, armax_key);
        case Crypt::codebreaker:
            devices::codebreaker::decrypt_v1(block.cheat.words);
            break;
        case Crypt::codebreaker7_common:
            devices::codebreaker::decrypt_common_v7(block.cheat.words);
            break;
        case Crypt::gameshark3:
        case Crypt::gameshark5: {
            devices::gameshark::Context context;
            context.decrypt_words(block.cheat.words, true);
            break;
        }
        case Crypt::raw:
        case Crypt::ar2_raw:
        case Crypt::max_raw:
        case Crypt::codebreaker_raw:
        case Crypt::gameshark_raw:
            break;
    }
    return std::nullopt;
}

void encrypt_block(CheatBlock& block, Crypt crypt, std::uint32_t ar2_key,
                   std::uint8_t gs3_key, std::uint32_t armax_key,
                   devices::action_replay::Context& ar2_context,
                   bool& ar2_key_emitted, bool ar2_raw_enable_output) {
    using namespace devices::action_replay;

    switch (crypt) {
        case Crypt::ar1:
            encrypt_ar1(block.cheat.words);
            break;
        case Crypt::ar2:
            if (ar2_raw_enable_output) {
                if (!ar2_key_emitted) {
                    prepend_ar2_raw_enable_code(block.cheat.words);
                    ar2_key_emitted = true;
                }
            } else {
                if (!ar2_key_emitted) {
                    prepend_ar2_key_code(block.cheat.words, ar2_key);
                    ar2_key_emitted = true;
                }
                ar2_context.encrypt_ar2(block.cheat.words);
            }
            break;
        case Crypt::armax:
            devices::armax::encrypt_full(block.cheat.words, armax_key);
            break;
        case Crypt::codebreaker:
            devices::codebreaker::encrypt_v1(block.cheat.words);
            break;
        case Crypt::codebreaker7_common:
            devices::codebreaker::encrypt_common_v7(block.cheat.words);
            break;
        case Crypt::gameshark3: {
            devices::gameshark::Context context;
            context.encrypt_words(block.cheat.words, gs3_key);
            break;
        }
        case Crypt::gameshark5: {
            devices::gameshark::Context context;
            context.encrypt_words(block.cheat.words, gs3_key);
            devices::gameshark::add_verifier(block.cheat.words);
            break;
        }
        case Crypt::raw:
        case Crypt::ar2_raw:
        case Crypt::max_raw:
        case Crypt::codebreaker_raw:
        case Crypt::gameshark_raw:
            break;
    }
}

bool crypt_is_encrypted(Crypt crypt) noexcept {
    switch (crypt) {
        case Crypt::ar1:
        case Crypt::ar2:
        case Crypt::armax:
        case Crypt::codebreaker:
        case Crypt::codebreaker7_common:
        case Crypt::gameshark3:
        case Crypt::gameshark5:
            return true;
        case Crypt::raw:
        case Crypt::ar2_raw:
        case Crypt::max_raw:
        case Crypt::codebreaker_raw:
        case Crypt::gameshark_raw:
            return false;
    }
    return false;
}

bool codebreaker_v1_wildcards_are_value_passthrough(
    const CheatBlock& block) noexcept {
    for (const formats::text::WildcardMask& wildcard : block.wildcards) {
        if (wildcard.word_index >= block.cheat.words.size()) return false;

        // CodeBreaker V1-V6 commands 0, 1, and 2 encrypt only the address.
        // Their value word is carried through unchanged, so value-nibble
        // wildcards can safely survive CB <-> CRAW text conversions.
        if ((block.cheat.words[wildcard.word_index] >> 28U) > 2U) return false;
    }
    return true;
}

bool can_preserve_codebreaker_v1_wildcards(
    const CheatBlock& block, Crypt input_crypt, Crypt output_crypt,
    Device input_device, Device output_device) noexcept {
    const auto is_cb_v1_family = [](Crypt crypt) noexcept {
        return crypt == Crypt::codebreaker || crypt == Crypt::codebreaker_raw;
    };

    return input_device == Device::codebreaker &&
           output_device == Device::codebreaker &&
           is_cb_v1_family(input_crypt) && is_cb_v1_family(output_crypt) &&
           codebreaker_v1_wildcards_are_value_passthrough(block);
}

void remove_armax_verifier(CheatBlock& block) {
    const std::size_t lines = devices::armax::read_verifier_lines(block.cheat.words);
    const std::size_t words = lines * 2U;
    if (words > block.cheat.words.size()) {
        throw std::invalid_argument("ARMAX verifier length exceeds the code block");
    }
    block.cheat.words.erase(
        block.cheat.words.begin(),
        block.cheat.words.begin() + static_cast<std::ptrdiff_t>(words));
}


[[nodiscard]] bool is_mgs_c_type_pointer_source(Crypt crypt) noexcept {
    return crypt == Crypt::codebreaker ||
           crypt == Crypt::codebreaker7_common ||
           crypt == Crypt::codebreaker_raw;
}

[[nodiscard]] std::size_t rewrite_mgs_c_type_pointers(CheatBlock& block) {
    std::size_t rewritten = 0U;
    std::vector<std::uint32_t> normalized;
    normalized.reserve(block.cheat.words.size());

    for (std::size_t index = 0U; index < block.cheat.words.size();) {
        const std::uint32_t first = block.cheat.words[index];
        if ((first >> 28U) == 0xCU && index + 1U < block.cheat.words.size()) {
            const std::uint32_t second = block.cheat.words[index + 1U];
            const std::uint32_t pointer_address = first & 0x01FFFFFFU;
            const std::uint32_t offset = (second >> 16U) & 0xFFFFU;
            const std::uint32_t value = second & 0xFFFFU;

            normalized.push_back(0x60000000U | pointer_address);
            normalized.push_back(value);
            normalized.push_back(0x00010001U);
            normalized.push_back(offset);
            ++rewritten;
            index += 2U;
            continue;
        }

        normalized.push_back(first);
        ++index;
    }

    if (rewritten != 0U) block.cheat.words = std::move(normalized);
    return rewritten;
}

void append_translation_warnings(Result& result,
                                 const translate::Report& report) {
    result.warnings.insert(result.warnings.end(), report.warnings.begin(),
                           report.warnings.end());
}

void finalize_binary_metadata(CheatBlock& block, Device output_device) {
    if (block.label && !block.label->empty()) block.cheat.name = *block.label;
    if (block.cheat.empty() || output_device == Device::armax) return;

    bool master = block.cheat.flags[Cheat::flag_master_code] != 0U;
    for (std::size_t index = 0U; index + 1U < block.cheat.word_count(); index += 2U) {
        const std::uint32_t type = block.cheat.words[index] >> 28U;
        if (type == 0xFU || type == 0x9U ||
            ((output_device == Device::ar1 || output_device == Device::ar2) &&
             type == 0x8U)) {
            master = true;
            break;
        }
        if (type >= 4U && type <= 6U && index + 3U < block.cheat.word_count()) {
            index += 2U;
        }
    }
    block.cheat.flags[Cheat::flag_master_code] = master ? 1U : 0U;
}

} // namespace

bool is_implemented(Crypt crypt) noexcept {
    switch (crypt) {
        case Crypt::raw:
        case Crypt::ar2_raw:
        case Crypt::max_raw:
        case Crypt::codebreaker_raw:
        case Crypt::gameshark_raw:
        case Crypt::ar1:
        case Crypt::ar2:
        case Crypt::armax:
        case Crypt::codebreaker:
        case Crypt::codebreaker7_common:
        case Crypt::gameshark3:
        case Crypt::gameshark5:
            return true;
    }
    return false;
}

bool is_implemented(CodeFormat format) noexcept {
    return is_implemented(crypt_for_format(format));
}

Result convert_text(std::string_view input, const Request& request) {
    if (!is_implemented(request.input_format)) {
        throw std::invalid_argument("The selected input format has not been ported yet");
    }
    if (!is_implemented(request.output_format)) {
        throw std::invalid_argument("The selected output format has not been ported yet");
    }
    if (request.output_format == CodeFormat::inline_headers) {
        throw std::invalid_argument("INLINE is an input-only format");
    }

    const bool inline_input = request.input_format == CodeFormat::inline_headers;
    const Crypt input_crypt = crypt_for_format(request.input_format);
    const Crypt output_crypt = crypt_for_format(request.output_format);
    const Device fixed_input_device = device_for_format(request.input_format);
    const Device output_device = device_for_format(request.output_format);

    std::vector<formats::text::CheatBlock> blocks;
    if (request.input_format == CodeFormat::pnach_raw) {
        blocks = formats::pnach::parse_text(input);
    } else if (request.input_format == CodeFormat::ps2_mips_r5900) {
        formats::mips_r5900::ParseResult parsed = formats::mips_r5900::parse_text(input);
        if (!parsed.errors.empty()) {
            std::string message = "PS2 MIPS parse error(s):";
            for (const std::string& error : parsed.errors) message += "\n" + error;
            throw std::invalid_argument(message);
        }
        blocks = std::move(parsed.blocks);
    } else if (inline_input) {
        blocks = formats::text::parse_inline_text(input);
    } else {
        blocks = formats::text::parse_text(input, input_crypt);
    }
    Result result;

    std::uint32_t input_ar2_key = request.input_ar2_key;
    std::uint32_t output_ar2_key = request.output_ar2_key;
    if (request.ar2_key != devices::action_replay::ar1_seed &&
        input_ar2_key == devices::action_replay::ar1_seed &&
        output_ar2_key == devices::action_replay::ar1_seed) {
        input_ar2_key = request.ar2_key;
        output_ar2_key = request.ar2_key;
    }

    const bool ar2_raw_enable_input =
        devices::action_replay::is_raw_enable_seed(input_ar2_key);
    const bool ar2_raw_enable_output =
        devices::action_replay::is_raw_enable_seed(output_ar2_key);
    devices::action_replay::Context ar2_input_context(input_ar2_key);
    devices::action_replay::Context ar2_output_context;
    bool ar2_key_emitted = !ar2_raw_enable_output &&
                           output_ar2_key == devices::action_replay::ar1_seed;
    std::uint32_t current_folder_id = 0U;
    std::uint32_t deterministic_nonce_offset = 0U;
    const auto next_armax_verifier_nonce = [&request, &deterministic_nonce_offset]() {
        if (request.armax_verifier_nonce.has_value()) {
            return *request.armax_verifier_nonce + deterministic_nonce_offset++;
        }
        return devices::armax::random_verifier_nonce();
    };

    for (CheatBlock& block : blocks) {
        const std::uint32_t input_seed_before = ar2_input_context.get_seed();
        const std::uint32_t output_seed_before = ar2_output_context.get_seed();
        const bool key_emitted_before = ar2_key_emitted;
        std::string stage = "prepare";
        Crypt block_input_crypt = input_crypt;
        bool input_decrypted = false;

        try {
            if (block.label && !block.label->empty()) block.cheat.name = *block.label;

            if (block.cheat.empty()) {
                if (output_device != Device::armax || !request.make_organizers ||
                    !block.label || block.label->empty()) {
                    continue;
                }

                stage = "create ARMAX organizer";
                devices::armax::make_folder(block.cheat.words,
                                            request.armax_game_id,
                                            request.armax_region,
                                            next_armax_verifier_nonce());
                devices::armax::finalize_cheat(block.cheat,
                                               request.armax_disc_hash,
                                               true, current_folder_id);
                stage = "encrypt output";
                encrypt_block(block, output_crypt, output_ar2_key,
                              request.gs3_key, request.armax_key,
                              ar2_output_context, ar2_key_emitted,
                              ar2_raw_enable_output);
                continue;
            }

            block_input_crypt = inline_input
                                    ? crypt_from_label(block.label.value_or(""))
                                          .value_or(Crypt::raw)
                                    : input_crypt;
            const Device block_input_device = inline_input
                                                  ? device_for_crypt(block_input_crypt)
                                                  : fixed_input_device;

            if ((block_input_crypt == Crypt::gameshark3 ||
                 block_input_crypt == Crypt::gameshark5) &&
                block.cheat.word_count() >= 2U) {
                const CodePair first{block.cheat.words[0],
                                     block.cheat.words[1]};
                if (devices::gameshark::looks_like_verifier(first) &&
                    !devices::gameshark::has_valid_verifier(
                        block.cheat.words)) {
                    const std::string name =
                        block.label.value_or("Unnamed Cheat");
                    result.warnings.emplace_back(
                        "GameShark/Xploder verifier checksum is invalid for \"" +
                        name +
                        "\"; the encrypted payload was accepted and a new "
                        "verifier will be generated when exporting GS5/XP5.");
                }
            }

            stage = "decrypt input";
            const auto metadata = decrypt_block(block, block_input_crypt,
                                                request.armax_key,
                                                ar2_input_context,
                                                ar2_raw_enable_input);
            input_decrypted = true;
            if (metadata) {
                result.detected_armax_game_id = metadata->game_id;
                result.detected_armax_region = metadata->region;
            }

            if (request.mgs_c_type_pointer_mode &&
                is_mgs_c_type_pointer_source(block_input_crypt)) {
                stage = "normalize MGS C-type pointers";
                const std::size_t rewritten = rewrite_mgs_c_type_pointers(block);
                if (rewritten != 0U) {
                    const std::string name = block.label.value_or("Unnamed Cheat");
                    result.warnings.emplace_back(
                        "MGS C-Type Pointer Mode converted " +
                        std::to_string(rewritten) +
                        " C-type pointer line(s) to CodeBreaker type 6 for \"" +
                        name + "\".");
                    if (!block.wildcards.empty()) {
                        block.wildcards.clear();
                        result.warnings.emplace_back(
                            "Wildcard masks were removed because MGS C-Type Pointer Mode changed code-word positions.");
                    }
                }
            }

            bool translated = false;
            if (block_input_device != output_device) {
                if (block_input_device == Device::armax &&
                    (block_input_crypt == Crypt::armax ||
                     request.armax_verifier_mode == ArmaxVerifierMode::manual)) {
                    stage = "remove ARMAX verifier";
                    remove_armax_verifier(block);
                }

                stage = "transpose code types";
                translate::Report report;
                try {
                    translate::translate_cheat(block.cheat, block_input_device,
                                               output_device, &report,
                                               request.transpose_mode);
                } catch (const translate::TranslationError& error) {
                    append_translation_warnings(result, report);
                    if (request.transpose_mode !=
                        translate::TransposeMode::original) {
                        throw;
                    }

                    block.cheat.state = 1U;
                    block.conversion_error = error.what();
                    const std::string name = block.label.value_or("Unnamed Cheat");
                    result.warnings.emplace_back(
                        "Original transpose kept the legacy partial result for \"" +
                        name + "\": " + error.what());
                    result.issues.push_back({name, stage, error.what()});
                    if (!block.wildcards.empty()) block.wildcards.clear();
                    continue;
                }
                append_translation_warnings(result, report);
                translated = true;
            }

            if (output_device == Device::armax) {
                const bool add_verifier =
                    (block_input_device != Device::armax &&
                     block.cheat.word_count() > 1U) ||
                    (request.armax_verifier_mode == ArmaxVerifierMode::automatic &&
                     block_input_crypt != Crypt::armax);
                if (add_verifier) {
                    stage = "create ARMAX verifier";
                    devices::armax::prepend_verifier(
                        block.cheat.words, request.armax_game_id,
                        request.armax_region, next_armax_verifier_nonce());
                    if (request.armax_game_id == 0U) {
                        result.warnings.emplace_back(
                            "ARMAX verifier was generated with Game ID 0000. Set the Game ID for console-ready output.");
                    }
                }
            }

            if (translated && !block.wildcards.empty()) {
                result.warnings.emplace_back(
                    "Wildcard masks were removed because semantic translation changed code-word positions.");
                block.wildcards.clear();
            }

            stage = "finalize output metadata";
            finalize_binary_metadata(block, output_device);
            if (output_device == Device::armax) {
                devices::armax::finalize_cheat(block.cheat,
                                               request.armax_disc_hash,
                                               request.make_organizers,
                                               current_folder_id);
            }

            stage = "encrypt output";
            encrypt_block(block, output_crypt, output_ar2_key,
                          request.gs3_key, request.armax_key,
                          ar2_output_context, ar2_key_emitted,
                          ar2_raw_enable_output);

            if (crypt_is_encrypted(block_input_crypt) ||
                code_format_is_encrypted(request.output_format)) {
                const bool preserve_cb_v1_wildcards =
                    !block.wildcards.empty() &&
                    can_preserve_codebreaker_v1_wildcards(
                        block, block_input_crypt, output_crypt,
                        block_input_device, output_device);
                if (!block.wildcards.empty() && !preserve_cb_v1_wildcards) {
                    result.warnings.emplace_back(
                        "Wildcard masks were removed because encrypted output requires concrete values.");
                    block.wildcards.clear();
                }
            }
        } catch (const std::exception& error) {
            if (!request.continue_on_error) throw;

            if (!input_decrypted) ar2_input_context.set_seed(input_seed_before);
            ar2_output_context.set_seed(output_seed_before);
            ar2_key_emitted = key_emitted_before;

            const std::string name = block.label.value_or(
                block.cheat.name.empty() ? "Unnamed Cheat" : block.cheat.name);
            std::string detail = "Error code: conversion\nStage: " + stage +
                                 "\nInput format: " +
                                 std::string(crypt_token(block_input_crypt)) +
                                 "\nTranspose mode: " +
                                 std::string(translate::transpose_mode_token(request.transpose_mode)) +
                                 "\nDetails: " + error.what();
            block.conversion_error = detail;
            block.cheat.state = 1U;
            block.cheat.words.clear();
            block.wildcards.clear();
            result.issues.push_back({name, stage, error.what()});
            result.warnings.emplace_back(
                "Skipped \"" + name + "\" after " + stage + ": " + error.what());
        }
    }

    if (input_crypt == Crypt::ar2 || inline_input) {
        result.active_ar2_seed = ar2_input_context.get_seed();
    }

    if (request.output_format == CodeFormat::pnach_raw) {
        formats::pnach::WriteOptions options;
        options.game_name = request.game_name;
        options.include_crc = request.pnach_include_crc;
        options.crc = request.pnach_crc;
        result.output_text = formats::pnach::write_text(blocks, options);
    } else if (request.output_format == CodeFormat::ps2_mips_r5900) {
        for (formats::text::CheatBlock& block : blocks) {
            if (!block.wildcards.empty()) {
                result.warnings.emplace_back(
                    "Wildcard masks were removed because PS2 MIPS output requires concrete instruction words.");
                block.wildcards.clear();
            }
        }
        result.output_text = formats::mips_r5900::write_text(blocks);
    } else {
        formats::text::WriteOptions options;
        options.cmp_output_mode = request.cmp_output_mode;
        result.output_text = formats::text::write_text(blocks, output_crypt, options);
    }
    result.blocks = blocks;
    return result;
}

} // namespace omni::convert
