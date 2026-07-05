#include "formats/binary/application_formats.hpp"

#include "formats/binary/binary_common.hpp"

#include <stdexcept>

namespace omni::formats::binary {

LoadResult load_application_file(ApplicationFormat format, const std::string& path) {
    const std::vector<std::uint8_t> bytes = common::read_file(path);
    switch (format) {
        case ApplicationFormat::armax_bin: return load_armax_bin(bytes);
        case ApplicationFormat::codebreaker_cbc: return load_cbc(bytes);
        case ApplicationFormat::gameshark_p2m: return load_p2m(bytes);
        case ApplicationFormat::swap_magic_bin: return load_swap_magic_bin(bytes);
    }
    throw std::invalid_argument("Unknown binary application format");
}

void write_application_file(ApplicationFormat format, const std::string& path,
                            const std::vector<text::CheatBlock>& blocks,
                            const WriteOptions& options) {
    std::vector<std::uint8_t> bytes;
    switch (format) {
        case ApplicationFormat::armax_bin:
            bytes = write_armax_bin(blocks, options);
            break;
        case ApplicationFormat::codebreaker_cbc:
            bytes = write_cbc(blocks, options);
            break;
        case ApplicationFormat::gameshark_p2m:
            bytes = write_p2m(blocks, options);
            break;
        case ApplicationFormat::swap_magic_bin:
            bytes = write_swap_magic_bin(blocks, options);
            break;
    }
    common::write_file(path, bytes);
}

std::string application_format_name(ApplicationFormat format) {
    switch (format) {
        case ApplicationFormat::armax_bin: return "AR MAX BIN";
        case ApplicationFormat::codebreaker_cbc: return "CodeBreaker CBC";
        case ApplicationFormat::gameshark_p2m: return "XP/GS P2M";
        case ApplicationFormat::swap_magic_bin: return "Swap Magic BIN";
    }
    return "Unknown";
}

} // namespace omni::formats::binary
