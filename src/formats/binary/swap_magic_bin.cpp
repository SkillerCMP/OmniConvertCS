#include "formats/binary/application_formats.hpp"

#include "devices/gameshark/gs3_file.hpp"
#include "formats/binary/binary_common.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace omni::formats::binary {
namespace {

constexpr std::size_t name_max = 100U;
constexpr std::uint32_t crc_mask = 0x5FA27ED6U;
constexpr std::size_t header_size = 20U;

std::string block_name(const text::CheatBlock& block) {
    std::string value = block.label.value_or(block.cheat.name);
    if (value.empty()) value = "Unnamed Cheat";
    if (value.size() > name_max) value.resize(name_max);
    return value;
}

std::string read_counted_ascii(common::Reader& reader, std::string_view field_name) {
    const std::uint8_t stored_length = reader.u8();
    if (stored_length == 0U) {
        throw std::runtime_error("Swap Magic " + std::string(field_name) +
                                 " has a zero-length string field");
    }
    if (reader.remaining() < stored_length) {
        throw std::runtime_error("Swap Magic " + std::string(field_name) +
                                 " runs past the end of the file");
    }

    const std::vector<std::uint8_t> bytes = reader.bytes(stored_length);
    const auto terminator = std::find(bytes.begin(), bytes.end(), 0U);
    if (terminator == bytes.end()) {
        throw std::runtime_error("Swap Magic " + std::string(field_name) +
                                 " is missing its NUL terminator");
    }
    if (terminator + 1 != bytes.end()) {
        throw std::runtime_error("Swap Magic " + std::string(field_name) +
                                 " length includes data after the NUL terminator");
    }
    return std::string(bytes.begin(), terminator);
}

} // namespace

LoadResult load_swap_magic_bin(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < header_size) {
        throw std::runtime_error("Swap Magic BIN file is too small to contain a header");
    }

    common::Reader header(bytes);
    const std::uint32_t stored_crc = header.u32le();
    const std::uint32_t payload_size = header.u32le();
    const std::uint32_t game_count = header.u32le();
    const std::uint32_t header_cheat_count = header.u32le();
    const std::uint32_t header_line_count = header.u32le();

    if (game_count == 0U) {
        throw std::runtime_error("Swap Magic BIN reports zero games");
    }
    if (game_count != 1U) {
        throw std::runtime_error(
            "Swap Magic BIN contains multiple games; this loader currently supports one game per file");
    }
    if (payload_size != bytes.size() - header_size) {
        throw std::runtime_error("Swap Magic BIN payload-size field does not match the file size");
    }

    const std::vector<std::uint8_t> payload(
        bytes.begin() + static_cast<std::ptrdiff_t>(header_size), bytes.end());
    const std::uint32_t expected_crc =
        crc_mask ^ devices::gameshark::file_crc32(payload);
    if (stored_crc != expected_crc) {
        throw std::runtime_error("Swap Magic BIN payload CRC is invalid");
    }

    common::Reader reader(payload);
    const std::uint16_t game_cheat_count = reader.u16le();

    LoadResult result;
    result.input_format = CodeFormat::swap_magic_coder3;
    result.game_name = read_counted_ascii(reader, "game name");
    result.blocks.reserve(game_cheat_count);

    std::uint32_t parsed_lines = 0U;
    for (std::uint16_t index = 0U; index < game_cheat_count; ++index) {
        const std::uint16_t line_count = reader.u16le();

        text::CheatBlock block;
        block.label = read_counted_ascii(reader, "cheat name");
        block.cheat.name = block.label.value_or("");
        block.cheat.words.reserve(static_cast<std::size_t>(line_count) * 2U);

        if (reader.remaining() < static_cast<std::size_t>(line_count) * 8U) {
            throw std::runtime_error("Swap Magic cheat data is truncated");
        }
        for (std::uint16_t line = 0U; line < line_count; ++line) {
            block.cheat.words.push_back(reader.u32le());
            block.cheat.words.push_back(reader.u32le());
        }
        parsed_lines += line_count;

        // The format does not serialize a dedicated master flag. Swap Magic
        // lists conventionally place the required master code first, which is
        // also what the writer enforces.
        if (index == 0U && !block.cheat.empty()) {
            block.cheat.flags[Cheat::flag_master_code] = 1U;
        }
        result.blocks.push_back(std::move(block));
    }

    if (reader.remaining() != 0U) {
        result.warnings.emplace_back("Swap Magic BIN contains trailing payload bytes");
    }
    if (header_cheat_count != game_cheat_count) {
        result.warnings.emplace_back(
            "Swap Magic BIN header cheat count does not match the game record");
    }
    if (header_line_count != parsed_lines) {
        result.warnings.emplace_back(
            "Swap Magic BIN header line count does not match the parsed cheats");
    }

    return result;
}

std::vector<std::uint8_t> write_swap_magic_bin(
    const std::vector<text::CheatBlock>& blocks, const WriteOptions& options) {
    std::vector<const text::CheatBlock*> active;
    std::size_t master_count = 0U;
    for (const text::CheatBlock& block : blocks) {
        if (block.cheat.flags[Cheat::flag_master_code] != 0U) ++master_count;
        if (block.cheat.state == 0U && !block.cheat.empty()) active.push_back(&block);
    }
    if (master_count == 0U) throw std::invalid_argument("Swap Magic BIN export requires a master code");
    if (master_count > 1U) throw std::invalid_argument("Swap Magic BIN export cannot contain more than one master code");
    if (active.empty()) throw std::invalid_argument("No valid cheats are available for Swap Magic BIN export");
    if (active.size() > 0xFFFFU) throw std::invalid_argument("Swap Magic BIN contains too many cheats");

    common::Writer payload;
    payload.u16le(static_cast<std::uint16_t>(active.size()));
    std::string game_name = options.game_name;
    if (game_name.size() > name_max) game_name.resize(name_max);
    if (game_name.size() >= 0xFFU) throw std::invalid_argument("Swap Magic game name is too long");
    payload.u8(static_cast<std::uint8_t>(game_name.size() + 1U));
    payload.ascii_z(game_name);

    std::uint32_t total_lines = 0U;
    for (const text::CheatBlock* block : active) {
        if ((block->cheat.word_count() & 1U) != 0U) {
            throw std::invalid_argument("Swap Magic BIN requires complete address/value pairs");
        }
        const std::size_t lines = block->cheat.word_count() / 2U;
        if (lines > 0xFFFFU) throw std::invalid_argument("A Swap Magic cheat contains too many lines");
        total_lines += static_cast<std::uint32_t>(lines);
        payload.u16le(static_cast<std::uint16_t>(lines));
        const std::string name = block_name(*block);
        payload.u8(static_cast<std::uint8_t>(name.size() + 1U));
        payload.ascii_z(name);
        for (const std::uint32_t word : block->cheat.words) payload.u32le(word);
    }

    const std::vector<std::uint8_t>& payload_bytes = payload.data();
    const std::uint32_t crc = crc_mask ^ devices::gameshark::file_crc32(payload_bytes);
    common::Writer output(header_size + payload_bytes.size());
    output.u32le(crc);
    output.u32le(static_cast<std::uint32_t>(payload_bytes.size()));
    output.u32le(1U);
    output.u32le(static_cast<std::uint32_t>(active.size()));
    output.u32le(total_lines);
    output.append(payload_bytes);
    return std::move(output).take();
}

} // namespace omni::formats::binary
