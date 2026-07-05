#include "formats/binary/binary_common.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace omni::formats::binary::common {
namespace {

std::u16string bytewise_utf16_fallback(std::string_view text) {
    std::u16string result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(static_cast<char16_t>(static_cast<unsigned char>(value)));
    }
    return result;
}

std::string low_byte_utf8_fallback(const std::u16string& text) {
    std::string result;
    result.reserve(text.size());
    for (const char16_t value : text) {
        result.push_back(static_cast<char>(static_cast<unsigned int>(value) & 0xFFU));
    }
    return result;
}

std::u16string utf8_to_utf16(std::string_view text) {
    std::u16string result;
    result.reserve(text.size());

    for (std::size_t index = 0U; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index]);
        std::uint32_t code_point = 0U;
        std::size_t length = 0U;

        if (first <= 0x7FU) {
            code_point = first;
            length = 1U;
        } else if (first >= 0xC2U && first <= 0xDFU) {
            code_point = first & 0x1FU;
            length = 2U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            code_point = first & 0x0FU;
            length = 3U;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            code_point = first & 0x07U;
            length = 4U;
        } else {
            return bytewise_utf16_fallback(text);
        }

        if (index + length > text.size()) return bytewise_utf16_fallback(text);
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0U) != 0x80U) return bytewise_utf16_fallback(text);
            code_point = (code_point << 6U) | (continuation & 0x3FU);
        }

        const bool overlong = (length == 2U && code_point < 0x80U) ||
                              (length == 3U && code_point < 0x800U) ||
                              (length == 4U && code_point < 0x10000U);
        if (overlong || code_point > 0x10FFFFU ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
            return bytewise_utf16_fallback(text);
        }

        if (code_point <= 0xFFFFU) {
            result.push_back(static_cast<char16_t>(code_point));
        } else {
            code_point -= 0x10000U;
            result.push_back(static_cast<char16_t>(0xD800U + (code_point >> 10U)));
            result.push_back(static_cast<char16_t>(0xDC00U + (code_point & 0x3FFU)));
        }
        index += length;
    }
    return result;
}

std::string utf16_to_utf8(const std::u16string& text) {
    std::string result;
    result.reserve(text.size() * 3U);

    for (std::size_t index = 0U; index < text.size(); ++index) {
        std::uint32_t code_point = static_cast<std::uint16_t>(text[index]);
        if (code_point >= 0xD800U && code_point <= 0xDBFFU) {
            if (index + 1U >= text.size()) return low_byte_utf8_fallback(text);
            const std::uint32_t low = static_cast<std::uint16_t>(text[index + 1U]);
            if (low < 0xDC00U || low > 0xDFFFU) return low_byte_utf8_fallback(text);
            code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);
            ++index;
        } else if (code_point >= 0xDC00U && code_point <= 0xDFFFU) {
            return low_byte_utf8_fallback(text);
        }

        if (code_point <= 0x7FU) {
            result.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7FFU) {
            result.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
            result.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else if (code_point <= 0xFFFFU) {
            result.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
            result.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else {
            result.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
            result.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            result.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
    }
    return result;
}

const std::array<std::uint32_t, 256>& crc_table() {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t index = 0; index < result.size(); ++index) {
            std::uint32_t value = index;
            for (unsigned bit = 0; bit < 8U; ++bit) {
                value = (value & 1U) != 0U
                    ? 0xEDB88320U ^ (value >> 1U)
                    : value >> 1U;
            }
            result[index] = value;
        }
        return result;
    }();
    return table;
}

} // namespace

void Reader::require(std::size_t count) const {
    if (offset_ > data_.size() || count > data_.size() - offset_) {
        throw std::runtime_error("Unexpected end of binary file");
    }
}

void Reader::seek(std::size_t offset) {
    if (offset > data_.size()) throw std::runtime_error("Binary seek is past end of file");
    offset_ = offset;
}

void Reader::skip(std::size_t count) {
    require(count);
    offset_ += count;
}

std::uint8_t Reader::u8() {
    require(1U);
    return data_[offset_++];
}

std::uint16_t Reader::u16le() {
    require(2U);
    const std::uint16_t value = static_cast<std::uint16_t>(data_[offset_]) |
                                static_cast<std::uint16_t>(data_[offset_ + 1U] << 8U);
    offset_ += 2U;
    return value;
}

std::uint32_t Reader::u32le() {
    require(4U);
    const std::uint32_t value = static_cast<std::uint32_t>(data_[offset_]) |
                                (static_cast<std::uint32_t>(data_[offset_ + 1U]) << 8U) |
                                (static_cast<std::uint32_t>(data_[offset_ + 2U]) << 16U) |
                                (static_cast<std::uint32_t>(data_[offset_ + 3U]) << 24U);
    offset_ += 4U;
    return value;
}

std::string Reader::ascii_z(std::size_t max_length) {
    const std::size_t end = max_length == static_cast<std::size_t>(-1)
                                ? data_.size()
                                : std::min(data_.size(), offset_ + max_length);
    std::string result;
    while (offset_ < end) {
        const std::uint8_t value = data_[offset_++];
        if (value == 0U) return result;
        result.push_back(static_cast<char>(value));
    }
    if (max_length == static_cast<std::size_t>(-1)) {
        throw std::runtime_error("Unterminated ASCII string in binary file");
    }
    return result;
}

std::string Reader::fixed_ascii(std::size_t length) {
    require(length);
    const std::size_t begin = offset_;
    std::size_t end = begin;
    while (end < begin + length && data_[end] != 0U) ++end;
    offset_ += length;
    return std::string(reinterpret_cast<const char*>(data_.data() + begin), end - begin);
}

std::string Reader::utf16le_z() {
    std::u16string units;
    while (true) {
        const std::uint16_t unit = u16le();
        if (unit == 0U) break;
        units.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(units);
}

std::vector<std::uint8_t> Reader::bytes(std::size_t count) {
    require(count);
    std::vector<std::uint8_t> result(data_.begin() + static_cast<std::ptrdiff_t>(offset_),
                                     data_.begin() + static_cast<std::ptrdiff_t>(offset_ + count));
    offset_ += count;
    return result;
}

void Writer::u8(std::uint8_t value) { data_.push_back(value); }

void Writer::u16le(std::uint16_t value) {
    data_.push_back(static_cast<std::uint8_t>(value));
    data_.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void Writer::u32le(std::uint32_t value) {
    data_.push_back(static_cast<std::uint8_t>(value));
    data_.push_back(static_cast<std::uint8_t>(value >> 8U));
    data_.push_back(static_cast<std::uint8_t>(value >> 16U));
    data_.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void Writer::ascii(std::string_view text) {
    data_.insert(data_.end(), text.begin(), text.end());
}

void Writer::ascii_z(std::string_view text) {
    ascii(text);
    u8(0U);
}

void Writer::fixed_ascii(std::string_view text, std::size_t length) {
    const std::size_t count = std::min(text.size(), length);
    data_.insert(data_.end(), text.begin(), text.begin() + static_cast<std::ptrdiff_t>(count));
    zeros(length - count);
}

void Writer::utf16le_z(std::string_view text, std::size_t max_units) {
    std::u16string units = utf8_to_utf16(text);
    if (units.size() > max_units) units.resize(max_units);
    for (const char16_t unit : units) u16le(static_cast<std::uint16_t>(unit));
    u16le(0U);
}

void Writer::append(const std::vector<std::uint8_t>& bytes) {
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

void Writer::append(const std::uint8_t* data, std::size_t count) {
    data_.insert(data_.end(), data, data + static_cast<std::ptrdiff_t>(count));
}

void Writer::zeros(std::size_t count) { data_.insert(data_.end(), count, 0U); }

void Writer::patch_u32le(std::size_t offset, std::uint32_t value) {
    if (offset > data_.size() || data_.size() - offset < 4U) {
        throw std::runtime_error("Binary patch offset is outside the output buffer");
    }
    data_[offset] = static_cast<std::uint8_t>(value);
    data_[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    data_[offset + 2U] = static_cast<std::uint8_t>(value >> 16U);
    data_[offset + 3U] = static_cast<std::uint8_t>(value >> 24U);
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open binary input file: " + path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Unable to create binary output file: " + path);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("Unable to write binary output file: " + path);
}

std::uint32_t crc32_standard(const std::uint8_t* data, std::size_t size,
                             std::uint32_t initial) {
    std::uint32_t crc = initial;
    const auto& table = crc_table();
    for (std::size_t index = 0; index < size; ++index) {
        crc = table[(crc & 0xFFU) ^ data[index]] ^ (crc >> 8U);
    }
    return crc ^ 0xFFFFFFFFU;
}

std::uint32_t crc32_standard(const std::vector<std::uint8_t>& data,
                             std::size_t offset, std::size_t size,
                             std::uint32_t initial) {
    if (offset > data.size() || size > data.size() - offset) {
        throw std::runtime_error("CRC range is outside the binary buffer");
    }
    return crc32_standard(data.data() + offset, size, initial);
}

std::string collapse_newlines(std::string text) {
    for (char& character : text) {
        if (character == '\r' || character == '\n') character = ' ';
    }
    return text;
}

} // namespace omni::formats::binary::common
