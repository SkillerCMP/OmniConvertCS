#include "core/crypt.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace omni {
namespace {

std::string normalize(std::string_view text) {
    std::string value(text);
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base(), value.end());

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    constexpr std::string_view prefix = "CRYPT_";
    if (value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0) {
        value.erase(0, prefix.size());
    }
    return value;
}

} // namespace

std::optional<Crypt> parse_crypt(std::string_view text) {
    const std::string value = normalize(text);
    if (value == "AR1") return Crypt::ar1;
    if (value == "AR2") return Crypt::ar2;
    if (value == "ARMAX") return Crypt::armax;
    if (value == "CB") return Crypt::codebreaker;
    if (value == "CB7_COMMON") return Crypt::codebreaker7_common;
    if (value == "GS3") return Crypt::gameshark3;
    if (value == "GS5") return Crypt::gameshark5;
    if (value == "MAXRAW") return Crypt::max_raw;
    if (value == "RAW") return Crypt::raw;
    if (value == "ARAW") return Crypt::ar2_raw;
    if (value == "CRAW") return Crypt::codebreaker_raw;
    if (value == "GRAW") return Crypt::gameshark_raw;
    return std::nullopt;
}

std::string_view crypt_token(Crypt crypt) noexcept {
    switch (crypt) {
        case Crypt::ar1: return "CRYPT_AR1";
        case Crypt::ar2: return "CRYPT_AR2";
        case Crypt::armax: return "CRYPT_ARMAX";
        case Crypt::codebreaker: return "CRYPT_CB";
        case Crypt::codebreaker7_common: return "CRYPT_CB7_COMMON";
        case Crypt::gameshark3: return "CRYPT_GS3";
        case Crypt::gameshark5: return "CRYPT_GS5";
        case Crypt::max_raw: return "CRYPT_MAXRAW";
        case Crypt::raw: return "CRYPT_RAW";
        case Crypt::ar2_raw: return "CRYPT_ARAW";
        case Crypt::codebreaker_raw: return "CRYPT_CRAW";
        case Crypt::gameshark_raw: return "CRYPT_GRAW";
    }
    return "CRYPT_RAW";
}

Device device_for_crypt(Crypt crypt) noexcept {
    switch (crypt) {
        case Crypt::ar1:
            return Device::ar1;
        case Crypt::ar2:
        case Crypt::ar2_raw:
            return Device::ar2;
        case Crypt::armax:
        case Crypt::max_raw:
            return Device::armax;
        case Crypt::codebreaker:
        case Crypt::codebreaker7_common:
        case Crypt::codebreaker_raw:
            return Device::codebreaker;
        case Crypt::gameshark3:
        case Crypt::gameshark5:
        case Crypt::gameshark_raw:
            return Device::gameshark3;
        case Crypt::raw:
            return Device::standard;
    }
    return Device::standard;
}

} // namespace omni
