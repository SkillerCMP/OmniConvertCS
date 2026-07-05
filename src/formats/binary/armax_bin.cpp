#include "formats/binary/application_formats.hpp"

#include "formats/binary/binary_common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace omni::formats::binary {
namespace {

constexpr std::string_view list_ident = "PS2_CODELIST";
constexpr std::uint32_t list_version = 0xD00DB00BU;
constexpr std::size_t list_header_size = 36U;
constexpr std::size_t minimum_cheat_bytes = 4U + 2U + 2U + 2U + 3U + 4U + 4U;

std::string block_name(const text::CheatBlock& block) {
    if (block.label && !block.label->empty()) return *block.label;
    if (!block.cheat.name.empty()) return block.cheat.name;
    return "Unnamed Cheat";
}

std::string legacy_comment(std::string value) {
    const std::size_t carriage_return = value.find('\r');
    if (carriage_return != std::string::npos) value.resize(carriage_return);
    if (value.size() > Cheat::name_max_size) value.resize(Cheat::name_max_size);
    return value;
}

std::string glyph_name(std::uint16_t unit) {
    switch (unit) {
        case 0x0003U: return "L3";
        case 0x0004U: return "R3";
        case 0x0010U: return "X";
        case 0x0011U: return "Square";
        case 0x0012U: return "Triangle";
        case 0x0013U: return "Circle";
        case 0x0014U: return "Select";
        case 0x0015U: return "Start";
        case 0x0016U: return "Up";
        case 0x0017U: return "Right";
        case 0x0018U: return "Down";
        case 0x0019U: return "Left";
        case 0x001AU: return "L1";
        case 0x001BU: return "L2";
        case 0x001CU: return "R1";
        case 0x001DU: return "R2";
        default: return {};
    }
}

std::string read_armax_wide(common::Reader& reader) {
    std::string result;
    while (true) {
        const std::uint16_t unit = reader.u16le();
        if (unit == 0U) break;
        const std::string glyph = glyph_name(unit);
        if (!glyph.empty()) {
            result += glyph;
        } else if (unit < 0x80U) {
            result.push_back(static_cast<char>(unit));
        } else if (unit < 0x800U) {
            result.push_back(static_cast<char>(0xC0U | (unit >> 6U)));
            result.push_back(static_cast<char>(0x80U | (unit & 0x3FU)));
        } else {
            result.push_back(static_cast<char>(0xE0U | (unit >> 12U)));
            result.push_back(static_cast<char>(0x80U | ((unit >> 6U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | (unit & 0x3FU)));
        }
    }
    return result;
}

bool reasonable_cheat_count(std::uint32_t count, std::size_t position,
                            std::size_t end) {
    if (count == 0U) return true;
    if (count > 10000U || position > end) return false;
    const std::uint64_t needed = static_cast<std::uint64_t>(count) * minimum_cheat_bytes;
    return needed <= static_cast<std::uint64_t>(end - position);
}

struct GameHeader {
    std::string note;
    std::uint32_t cheat_count{};
    std::size_t cheat_position{};
};

GameHeader read_game_header(common::Reader& reader, std::size_t end) {
    GameHeader header;
    header.note = read_armax_wide(reader);
    header.cheat_count = reader.u32le();
    const std::size_t no_extra = reader.position();
    const bool no_extra_ok = reasonable_cheat_count(header.cheat_count, no_extra, end);

    bool with_extra_ok = false;
    std::size_t with_extra = no_extra;
    if (reader.remaining() >= 4U) {
        with_extra += 4U;
        with_extra_ok = reasonable_cheat_count(header.cheat_count, with_extra, end);
    }
    if (!no_extra_ok && !with_extra_ok) {
        throw std::runtime_error("Unsupported or corrupt AR MAX game header");
    }
    header.cheat_position = no_extra_ok ? no_extra : with_extra;
    reader.seek(header.cheat_position);
    return header;
}


} // namespace

LoadResult load_armax_bin(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < list_header_size) {
        throw std::runtime_error("File is too small to be a valid AR MAX list");
    }

    common::Reader reader(bytes);
    const std::string ident = reader.fixed_ascii(12U);
    if (ident != list_ident) {
        throw std::runtime_error("File does not start with PS2_CODELIST header");
    }
    const std::uint32_t version = reader.u32le();
    static_cast<void>(version);
    static_cast<void>(reader.u32le()); // unknown
    const std::uint32_t list_size = reader.u32le();
    const std::uint32_t stored_crc = reader.u32le();
    const std::uint32_t game_count = reader.u32le();
    static_cast<void>(reader.u32le()); // total code count
    if (game_count == 0U) throw std::runtime_error("AR MAX list reports zero games");

    LoadResult result;
    result.input_format = CodeFormat::action_replay_max;

    if (list_size <= bytes.size() - 28U) {
        const std::uint32_t calculated = common::crc32_standard(bytes, 28U, list_size);
        if (calculated != stored_crc) {
            result.warnings.emplace_back("AR MAX list CRC does not match the file contents");
        }
    }

    for (std::uint32_t game_index = 0U; game_index < game_count; ++game_index) {
        const std::uint32_t game_id = reader.u32le();
        const std::string game_name = read_armax_wide(reader);
        const GameHeader game_header = read_game_header(reader, bytes.size());
        std::string full_name = game_name;
        if (!game_header.note.empty()) {
            full_name += " {" + common::collapse_newlines(game_header.note) + "}";
        }

        if (game_count == 1U) {
            result.game_name = full_name;
            result.game_id = game_id;
        } else {
            text::CheatBlock heading;
            heading.label = "Game Name: " + full_name;
            result.blocks.push_back(std::move(heading));
        }

        for (std::uint32_t cheat_index = 0U; cheat_index < game_header.cheat_count;
             ++cheat_index) {
            text::CheatBlock block;
            block.cheat.id = reader.u32le();
            std::string name = read_armax_wide(reader);
            const std::string comment = read_armax_wide(reader);
            if (!comment.empty()) {
                const std::string inline_comment = common::collapse_newlines(comment);
                name = name.empty() ? "{" + inline_comment + "}"
                                    : name + " {" + inline_comment + "}";
                block.cheat.comment = comment;
            }
            block.label = name;
            block.cheat.name = name;
            block.cheat.flags[Cheat::flag_default_on] = reader.u8();
            block.cheat.flags[Cheat::flag_master_code] = reader.u8();
            block.cheat.flags[Cheat::flag_comments] = reader.u8();
            const std::uint32_t word_count = reader.u32le();
            if (word_count > reader.remaining() / 4U) {
                throw std::runtime_error("Truncated code data in AR MAX cheat");
            }
            block.cheat.words.reserve(word_count);
            for (std::uint32_t word = 0U; word < word_count; ++word) {
                block.cheat.words.push_back(reader.u32le());
            }
            block.has_armax_encrypted_line = true;
            result.blocks.push_back(std::move(block));
        }
    }
    return result;
}

std::vector<std::uint8_t> write_armax_bin(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options) {
    std::vector<const text::CheatBlock*> valid;
    bool has_master = false;
    for (const text::CheatBlock& block : blocks) {
        if (block.cheat.state != 0U || block.cheat.empty()) continue;
        valid.push_back(&block);
        has_master = has_master || block.cheat.flags[Cheat::flag_master_code] != 0U;
    }
    if (valid.empty()) throw std::invalid_argument("No valid cheats are available for AR MAX BIN export");
    if (!has_master) throw std::invalid_argument("AR MAX BIN export requires a master code");

    common::Writer payload;
    payload.u32le(options.game_id);
    payload.utf16le_z(options.game_name.empty() ? "Unnamed Game" : options.game_name,
                      Cheat::name_max_size);
    payload.u16le(0U); // empty game note / extra wchar
    payload.u32le(static_cast<std::uint32_t>(valid.size()));

    for (const text::CheatBlock* block : valid) {
        payload.u32le(block->cheat.id);
        payload.utf16le_z(block_name(*block), Cheat::name_max_size);
        const std::string comment = legacy_comment(block->cheat.comment);
        payload.utf16le_z(comment, Cheat::name_max_size);
        payload.u8(block->cheat.flags[Cheat::flag_default_on]);
        payload.u8(block->cheat.flags[Cheat::flag_master_code]);
        // Preserve the OmniConvertCS writer's legacy behavior: the comment
        // flag is set even when the serialized comment is empty.
        payload.u8(1U);
        payload.u32le(static_cast<std::uint32_t>(block->cheat.word_count()));
        for (const std::uint32_t word : block->cheat.words) payload.u32le(word);
    }

    common::Writer output(list_header_size + payload.data().size());
    output.fixed_ascii(list_ident, 12U);
    output.u32le(list_version);
    output.u32le(0U);
    const std::uint32_t size_field = static_cast<std::uint32_t>(payload.data().size() + 8U);
    output.u32le(size_field);
    const std::size_t crc_offset = output.position();
    output.u32le(0U);
    output.u32le(1U);
    output.u32le(static_cast<std::uint32_t>(valid.size()));
    output.append(payload.data());

    std::vector<std::uint8_t> bytes = std::move(output).take();
    const std::uint32_t crc = common::crc32_standard(bytes, 28U, size_field);
    bytes[crc_offset] = static_cast<std::uint8_t>(crc);
    bytes[crc_offset + 1U] = static_cast<std::uint8_t>(crc >> 8U);
    bytes[crc_offset + 2U] = static_cast<std::uint8_t>(crc >> 16U);
    bytes[crc_offset + 3U] = static_cast<std::uint8_t>(crc >> 24U);
    return bytes;
}

} // namespace omni::formats::binary
