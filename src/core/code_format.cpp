#include "core/code_format.hpp"

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
        if (c == '-' || c == '/' || c == ' ') return '_';
        return static_cast<char>(std::toupper(c));
    });

    constexpr std::string_view crypt_prefix = "CRYPT_";
    if (value.size() >= crypt_prefix.size() &&
        value.compare(0, crypt_prefix.size(), crypt_prefix) == 0) {
        value.erase(0, crypt_prefix.size());
    }

    constexpr std::string_view format_prefix = "FORMAT_";
    if (value.size() >= format_prefix.size() &&
        value.compare(0, format_prefix.size(), format_prefix) == 0) {
        value.erase(0, format_prefix.size());
    }

    return value;
}

} // namespace

std::optional<CodeFormat> parse_code_format(std::string_view text) {
    const std::string value = normalize(text);

    if (value == "RAW" || value == "STANDARD" || value == "STANDARD_RAW") {
        return CodeFormat::standard_raw;
    }
    if (value == "INLINE" || value == "INLINE_HEADERS" ||
        value == "INLINE_CRYPT") {
        return CodeFormat::inline_headers;
    }
    if (value == "PNACH" || value == "PNACH_RAW" || value == "PNACH(RAW)") {
        return CodeFormat::pnach_raw;
    }
    if (value == "MIPS" || value == "R5900" || value == "PS2_MIPS" ||
        value == "PS2_MIPS_R5900" || value == "MIPS_R5900") {
        return CodeFormat::ps2_mips_r5900;
    }
    if (value == "MAXRAW" || value == "ARMAX_RAW") return CodeFormat::armax_raw;
    if (value == "ARAW" || value == "AR12_RAW" || value == "AR_GS12_RAW") {
        return CodeFormat::ar12_raw;
    }
    if (value == "CRAW" || value == "CBRAW" || value == "CODEBREAKER_RAW") {
        return CodeFormat::codebreaker_raw;
    }
    if (value == "GS12RAW" || value == "GS12_RAW" || value == "GAMESHARK12_RAW") {
        return CodeFormat::gameshark12_raw;
    }
    if (value == "GRAW" || value == "GS3RAW" || value == "GS3_RAW" ||
        value == "XP_GS3_RAW") {
        return CodeFormat::gameshark3_raw;
    }

    if (value == "AR1" || value == "AR_V1") return CodeFormat::action_replay1;
    if (value == "AR2" || value == "AR_V2") return CodeFormat::action_replay2;
    if (value == "ARMAX" || value == "MAX") return CodeFormat::action_replay_max;
    if (value == "CB" || value == "CB1_6" || value == "CODEBREAKER") {
        return CodeFormat::codebreaker1_6;
    }
    if (value == "CB7_COMMON" || value == "CB7COMMON") {
        return CodeFormat::codebreaker7_common;
    }
    if (value == "GS1" || value == "IA1" || value == "INTERACT_GS1") {
        return CodeFormat::gameshark_interact1;
    }
    if (value == "GS2" || value == "IA2" || value == "INTERACT_GS2") {
        return CodeFormat::gameshark_interact2;
    }
    if (value == "GS3" || value == "GS34" || value == "MADCATZ_GS34") {
        return CodeFormat::gameshark_madcatz34;
    }
    if (value == "GS5" || value == "MADCATZ_GS5") {
        return CodeFormat::gameshark_madcatz5;
    }
    if (value == "SMC" || value == "SWAP_MAGIC" || value == "SWAP_MAGIC_CODER3") {
        return CodeFormat::swap_magic_coder3;
    }
    if (value == "XP13" || value == "XP1_3" || value == "XP1" ||
        value == "XP2" || value == "XP3" || value == "XPLODER1_3") {
        return CodeFormat::xploder1_3;
    }
    if (value == "XP4" || value == "XPLODER4") return CodeFormat::xploder4;
    if (value == "XP5" || value == "XPLODER5") return CodeFormat::xploder5;

    return std::nullopt;
}

std::string_view code_format_token(CodeFormat format) noexcept {
    switch (format) {
        case CodeFormat::standard_raw: return "RAW";
        case CodeFormat::inline_headers: return "INLINE";
        case CodeFormat::pnach_raw: return "PNACH";
        case CodeFormat::ps2_mips_r5900: return "MIPS";
        case CodeFormat::armax_raw: return "MAXRAW";
        case CodeFormat::ar12_raw: return "ARAW";
        case CodeFormat::codebreaker_raw: return "CRAW";
        case CodeFormat::gameshark12_raw: return "GS12RAW";
        case CodeFormat::gameshark3_raw: return "GRAW";
        case CodeFormat::action_replay1: return "AR1";
        case CodeFormat::action_replay2: return "AR2";
        case CodeFormat::action_replay_max: return "ARMAX";
        case CodeFormat::codebreaker1_6: return "CB";
        case CodeFormat::codebreaker7_common: return "CB7_COMMON";
        case CodeFormat::gameshark_interact1: return "GS1";
        case CodeFormat::gameshark_interact2: return "GS2";
        case CodeFormat::gameshark_madcatz34: return "GS3";
        case CodeFormat::gameshark_madcatz5: return "GS5";
        case CodeFormat::swap_magic_coder3: return "SMC";
        case CodeFormat::xploder1_3: return "XP13";
        case CodeFormat::xploder4: return "XP4";
        case CodeFormat::xploder5: return "XP5";
    }
    return "RAW";
}

std::string_view code_format_name(CodeFormat format) noexcept {
    switch (format) {
        case CodeFormat::standard_raw: return "RAW / Standard";
        case CodeFormat::inline_headers: return "INLINE (CRYPT_ headers)";
        case CodeFormat::pnach_raw: return "PNACH (RAW)";
        case CodeFormat::ps2_mips_r5900: return "PS2 MIPS (R5900)";
        case CodeFormat::armax_raw: return "ARMAX RAW";
        case CodeFormat::ar12_raw: return "Action Replay / GameShark V1-V2 RAW";
        case CodeFormat::codebreaker_raw: return "CodeBreaker RAW";
        case CodeFormat::gameshark12_raw: return "GameShark V1-V2 RAW";
        case CodeFormat::gameshark3_raw: return "Xploder / GameShark V3 RAW";
        case CodeFormat::action_replay1: return "Action Replay V1";
        case CodeFormat::action_replay2: return "Action Replay V2";
        case CodeFormat::action_replay_max: return "Action Replay MAX";
        case CodeFormat::codebreaker1_6: return "CodeBreaker V1-V6";
        case CodeFormat::codebreaker7_common: return "CodeBreaker V7+ Common";
        case CodeFormat::gameshark_interact1: return "Interact GameShark V1";
        case CodeFormat::gameshark_interact2: return "Interact GameShark V2";
        case CodeFormat::gameshark_madcatz34: return "MadCatz GameShark V3-V4";
        case CodeFormat::gameshark_madcatz5: return "MadCatz GameShark V5+";
        case CodeFormat::swap_magic_coder3: return "Swap Magic Coder V3.x";
        case CodeFormat::xploder1_3: return "Xploder V1-V3";
        case CodeFormat::xploder4: return "Xploder V4";
        case CodeFormat::xploder5: return "Xploder V5+";
    }
    return "Unknown";
}

Crypt crypt_for_format(CodeFormat format) noexcept {
    switch (format) {
        case CodeFormat::standard_raw:
        case CodeFormat::inline_headers:
        case CodeFormat::pnach_raw:
        case CodeFormat::ps2_mips_r5900:
            return Crypt::raw;
        case CodeFormat::armax_raw:
            return Crypt::max_raw;
        case CodeFormat::ar12_raw:
        case CodeFormat::gameshark12_raw:
            return Crypt::ar2_raw;
        case CodeFormat::codebreaker_raw:
            return Crypt::codebreaker_raw;
        case CodeFormat::gameshark3_raw:
            return Crypt::gameshark_raw;
        case CodeFormat::action_replay1:
        case CodeFormat::gameshark_interact1:
        case CodeFormat::swap_magic_coder3:
            return Crypt::ar1;
        case CodeFormat::action_replay2:
        case CodeFormat::gameshark_interact2:
            return Crypt::ar2;
        case CodeFormat::action_replay_max:
            return Crypt::armax;
        case CodeFormat::codebreaker1_6:
        case CodeFormat::xploder1_3:
            return Crypt::codebreaker;
        case CodeFormat::codebreaker7_common:
            return Crypt::codebreaker7_common;
        case CodeFormat::gameshark_madcatz34:
        case CodeFormat::xploder4:
            return Crypt::gameshark3;
        case CodeFormat::gameshark_madcatz5:
        case CodeFormat::xploder5:
            return Crypt::gameshark5;
    }
    return Crypt::raw;
}

Device device_for_format(CodeFormat format) noexcept {
    switch (format) {
        case CodeFormat::standard_raw:
        case CodeFormat::inline_headers:
        case CodeFormat::pnach_raw:
        case CodeFormat::ps2_mips_r5900:
            return Device::standard;
        case CodeFormat::armax_raw:
        case CodeFormat::action_replay_max:
            return Device::armax;
        case CodeFormat::action_replay1:
        case CodeFormat::gameshark_interact1:
        case CodeFormat::swap_magic_coder3:
            return Device::ar1;
        case CodeFormat::ar12_raw:
        case CodeFormat::gameshark12_raw:
        case CodeFormat::action_replay2:
        case CodeFormat::gameshark_interact2:
            return Device::ar2;
        case CodeFormat::codebreaker_raw:
        case CodeFormat::codebreaker1_6:
        case CodeFormat::codebreaker7_common:
        case CodeFormat::xploder1_3:
            return Device::codebreaker;
        case CodeFormat::gameshark3_raw:
        case CodeFormat::gameshark_madcatz34:
        case CodeFormat::gameshark_madcatz5:
        case CodeFormat::xploder4:
        case CodeFormat::xploder5:
            return Device::gameshark3;
    }
    return Device::standard;
}

bool code_format_is_encrypted(CodeFormat format) noexcept {
    switch (crypt_for_format(format)) {
        case Crypt::ar1:
        case Crypt::ar2:
        case Crypt::armax:
        case Crypt::codebreaker:
        case Crypt::codebreaker7_common:
        case Crypt::gameshark3:
        case Crypt::gameshark5:
            return true;
        case Crypt::max_raw:
        case Crypt::raw:
        case Crypt::ar2_raw:
        case Crypt::codebreaker_raw:
        case Crypt::gameshark_raw:
            return false;
    }
    return false;
}

} // namespace omni
