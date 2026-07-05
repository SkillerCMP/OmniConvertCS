#include "translate/original_translation_helpers.hpp"

namespace omni::translate::detail {

void original_smash(Cheat& destination, std::uint8_t size,
                    std::uint32_t address_value, std::uint32_t value,
                    std::uint32_t fill_count) {
    std::uint32_t bytes = fill_count << size;
    if (fill_count == 1U) {
        append_pair(destination, make_std(size) | address_value, value);
        return;
    }

    while (bytes != 0U) {
        if ((address_value % 4U) == 0U) {
            if (bytes > 12U) {
                append_pair(destination,
                            make_std(std_serial_word) | address_value,
                            (((bytes - (bytes % 4U)) / 4U) << 16U) | 1U);
                append_pair(destination, original_make_word(value, size), 0U);
                const std::uint32_t consumed = bytes - (bytes % 4U);
                address_value += consumed;
                bytes -= consumed;
            } else if (bytes >= 4U) {
                append_pair(destination,
                            make_std(size_word) | address_value,
                            original_make_word(value, size));
                address_value += 4U;
                bytes -= 4U;
            } else if (bytes >= 2U) {
                append_pair(destination,
                            make_std(size_half) | address_value,
                            original_make_half(value, size));
                address_value += 2U;
                bytes -= 2U;
            } else {
                append_pair(destination,
                            make_std(size_byte) | address_value, value);
                ++address_value;
                --bytes;
            }
        } else if ((address_value % 2U) == 0U) {
            if (bytes >= 2U) {
                append_pair(destination,
                            make_std(size_half) | address_value,
                            original_make_half(value, size));
                address_value += 2U;
                bytes -= 2U;
            } else {
                append_pair(destination,
                            make_std(size_byte) | address_value, value);
                ++address_value;
                --bytes;
            }
        } else {
            append_pair(destination,
                        make_std(size_byte) | address_value, value);
            ++address_value;
            --bytes;
        }
    }
}

void original_explode(Cheat& destination, std::uint8_t size,
                      std::uint32_t address_value, std::uint32_t value,
                      std::uint32_t fill_count, std::uint8_t skip,
                      std::uint8_t increment) {
    if (skip == 1U && increment == 0U) {
        original_smash(destination, size, address_value, value, fill_count);
        return;
    }
    if (fill_count == 1U) {
        append_pair(destination, make_std(size) | address_value, value);
        return;
    }

    while (fill_count-- != 0U) {
        append_pair(destination, make_std(size) | address_value, value);
        address_value += (1U << size) * skip;
        value += increment;
    }
}

} // namespace omni::translate::detail
