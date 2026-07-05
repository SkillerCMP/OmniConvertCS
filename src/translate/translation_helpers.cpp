#include "translate/translation_internal.hpp"

#include <algorithm>
#include <limits>

namespace omni::translate::detail {

bool is_ar_family(Device device) noexcept {
    return device == Device::ar1 || device == Device::ar2;
}

bool is_cb(Device device) noexcept {
    return device == Device::codebreaker;
}

bool is_gs(Device device) noexcept {
    return device == Device::gameshark3;
}

bool is_standard_family(Device device) noexcept {
    return device != Device::armax;
}

void require_words(const std::vector<std::uint32_t>& words, std::size_t index,
                   std::size_t count) {
    if (index > words.size() || count > words.size() - index) {
        throw TranslationError(ErrorCode::invalid_code, index);
    }
}

void append_pair(Cheat& destination, std::uint32_t first, std::uint32_t second) {
    destination.append_pair(first, second);
}

void append_warning(Report* report, std::string message) {
    if (report != nullptr) report->warnings.push_back(std::move(message));
}

namespace {

std::uint32_t repeated_word(std::uint32_t value, std::uint8_t size) noexcept {
    if (size == size_byte) {
        value &= 0xFFU;
        return value | (value << 8U) | (value << 16U) | (value << 24U);
    }
    value &= 0xFFFFU;
    return value | (value << 16U);
}

std::uint32_t repeated_half(std::uint32_t value, std::uint8_t size) noexcept {
    if (size == size_byte) {
        value &= 0xFFU;
        return value | (value << 8U);
    }
    return value & 0xFFFFU;
}

} // namespace

void emit_smash(Cheat& destination, std::uint8_t size,
                std::uint32_t address_value, std::uint32_t value,
                std::uint32_t count) {
    if (count == 0U) return;
    if (count == 1U) {
        append_pair(destination, make_std(size) | address_value,
                    value & value_masks[size]);
        return;
    }

    std::uint64_t bytes64 = static_cast<std::uint64_t>(count) << size;
    if (bytes64 > std::numeric_limits<std::uint32_t>::max()) {
        throw TranslationError(ErrorCode::count_overflow, 0U,
                               "expanded fill is too large");
    }
    std::uint32_t bytes = static_cast<std::uint32_t>(bytes64);

    while (bytes != 0U) {
        if (address_value > address_mask) {
            throw TranslationError(
                ErrorCode::serial_size, 0U,
                "expanded fill requires another standard command whose start address exceeds the 25-bit field");
        }

        if ((address_value & 3U) == 0U) {
            if (bytes > 12U) {
                const std::uint32_t available_words = bytes / 4U;
                const std::uint32_t words =
                    std::min(available_words, 0xFFFFU);
                append_pair(destination,
                            make_std(std_serial_word) | address_value,
                            (words << 16U) | 1U);
                append_pair(destination, repeated_word(value, size), 0U);
                address_value += words * 4U;
                bytes -= words * 4U;
            } else if (bytes >= 4U) {
                append_pair(destination,
                            make_std(std_write_word) | address_value,
                            repeated_word(value, size));
                address_value += 4U;
                bytes -= 4U;
            } else if (bytes >= 2U) {
                append_pair(destination,
                            make_std(std_write_half) | address_value,
                            repeated_half(value, size));
                address_value += 2U;
                bytes -= 2U;
            } else {
                append_pair(destination,
                            make_std(std_write_byte) | address_value,
                            value & 0xFFU);
                ++address_value;
                --bytes;
            }
        } else if ((address_value & 1U) == 0U) {
            if (bytes >= 2U) {
                append_pair(destination,
                            make_std(std_write_half) | address_value,
                            repeated_half(value, size));
                address_value += 2U;
                bytes -= 2U;
            } else {
                append_pair(destination,
                            make_std(std_write_byte) | address_value,
                            value & 0xFFU);
                ++address_value;
                --bytes;
            }
        } else {
            append_pair(destination,
                        make_std(std_write_byte) | address_value,
                        value & 0xFFU);
            ++address_value;
            --bytes;
        }
    }
}

void emit_exploded(Cheat& destination, std::uint8_t size,
                   std::uint32_t address_value, std::uint32_t value,
                   std::uint32_t count, std::int32_t byte_step,
                   std::int32_t value_increment) {
    if (count == 0U) return;
    if (byte_step == static_cast<std::int32_t>(1U << size) &&
        value_increment == 0) {
        emit_smash(destination, size, address_value, value, count);
        return;
    }

    for (std::uint32_t i = 0U; i < count; ++i) {
        append_pair(destination, make_std(size) | (address_value & address_mask),
                    value & value_masks[size]);
        address_value = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(address_value) + byte_step);
        value = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(value) + value_increment);
    }
}

} // namespace omni::translate::detail
