#pragma once

#include <cstdint>
#include <type_traits>

namespace omni::bits {

template <typename T>
[[nodiscard]] constexpr T mask(unsigned width) noexcept {
    static_assert(std::is_unsigned_v<T>, "bit mask type must be unsigned");
    constexpr unsigned digits = static_cast<unsigned>(sizeof(T) * 8U);
    if (width == 0U) return T{0};
    if (width >= digits) return ~T{0};
    return static_cast<T>((T{1} << width) - T{1});
}

template <typename T>
[[nodiscard]] constexpr T extract(T value, unsigned offset, unsigned width) noexcept {
    static_assert(std::is_unsigned_v<T>, "bit field type must be unsigned");
    return static_cast<T>((value >> offset) & mask<T>(width));
}

template <typename T>
[[nodiscard]] constexpr T insert(T base, T field, unsigned offset, unsigned width) noexcept {
    static_assert(std::is_unsigned_v<T>, "bit field type must be unsigned");
    const T field_mask = static_cast<T>(mask<T>(width) << offset);
    return static_cast<T>((base & ~field_mask) | ((field << offset) & field_mask));
}

} // namespace omni::bits
