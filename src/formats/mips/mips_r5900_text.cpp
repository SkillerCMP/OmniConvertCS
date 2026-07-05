#include "formats/mips/mips_r5900_text.hpp"

#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace omni::formats::mips_r5900 {
namespace {

enum class ItemKind { instruction, word, half, byte, raw_code };

struct SourceItem {
    ItemKind kind{ItemKind::instruction};
    int line_number{};
    std::uint32_t address{};
    std::string text;
    std::uint32_t raw_address{};
    std::uint32_t raw_value{};
};

struct SourceBlock {
    std::optional<std::string> name;
    std::vector<SourceItem> items;
    std::unordered_map<std::string, std::uint32_t> labels;
};

constexpr std::array<std::string_view, 32> register_names{{
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
}};

std::string lower_copy(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool starts_with_ci(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() &&
           lower_copy(text.substr(0, prefix.size())) == lower_copy(prefix);
}

std::string format_hex(std::uint64_t value, int width) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << value;
    return out.str();
}

std::string first_token(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) == 0) ++index;
    return std::string(text.substr(0, index));
}

std::vector<std::string> split_operands(std::string_view text) {
    std::vector<std::string> result;
    if (hex::trim(text).empty()) return result;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char c = text[index];
        if (c == '(') ++depth;
        else if (c == ')') depth = std::max(0, depth - 1);
        else if (c == ',' && depth == 0) {
            const std::string part = hex::trim(text.substr(start, index - start));
            if (!part.empty()) result.push_back(part);
            start = index + 1U;
        }
    }
    const std::string part = hex::trim(text.substr(start));
    if (!part.empty()) result.push_back(part);
    return result;
}

std::string strip_inline_comment(std::string_view line) {
    std::size_t cut = line.size();
    const std::size_t slash = line.find("//");
    if (slash != std::string_view::npos) cut = std::min(cut, slash);
    const std::size_t semicolon = line.find(';');
    if (semicolon != std::string_view::npos) cut = std::min(cut, semicolon);
    const std::size_t hash = line.find('#');
    if (hash != std::string_view::npos) cut = std::min(cut, hash);
    return std::string(line.substr(0, cut));
}

const std::unordered_map<std::string, int>& register_map() {
    static const std::unordered_map<std::string, int> map = [] {
        std::unordered_map<std::string, int> value;
        for (int index = 0; index < static_cast<int>(register_names.size()); ++index) {
            const std::string name(register_names[static_cast<std::size_t>(index)]);
            value.emplace(lower_copy(name), index);
            value.emplace(lower_copy(name.substr(1)), index);
            value.emplace("$" + std::to_string(index), index);
            value.emplace("r" + std::to_string(index), index);
            value.emplace("$r" + std::to_string(index), index);
        }
        value["$s8"] = 30;
        value["s8"] = 30;
        return value;
    }();
    return map;
}

const std::unordered_set<std::string>& known_mnemonics() {
    static const std::unordered_set<std::string> values{
        "nop", "move", "clear", "li", "b", "bal", "beqz", "bnez", "negu", "not",
        "sll", "srl", "sra", "sllv", "srlv", "srav", "dsllv", "dsrlv", "dsrav",
        "jr", "jalr", "movz", "movn", "syscall", "break", "sync",
        "mfhi", "mthi", "mflo", "mtlo", "mfsa", "mtsa", "mtsab", "mtsah",
        "mult", "multu", "div", "divu", "madd", "maddu", "madd1", "maddu1",
        "mult1", "multu1", "div1", "divu1", "mfhi1", "mflo1", "mthi1", "mtlo1", "plzcw",
        "tge", "tgeu", "tlt", "tltu", "teq", "tne",
        "tgei", "tgeiu", "tlti", "tltiu", "teqi", "tnei",
        "add", "addu", "sub", "subu", "and", "or", "xor", "nor", "slt", "sltu",
        "dadd", "daddu", "dsub", "dsubu", "dsll", "dsrl", "dsra", "dsll32", "dsrl32", "dsra32",
        "addi", "addiu", "daddi", "daddiu", "slti", "sltiu", "andi", "ori", "xori", "lui", "ldl", "ldr",
        "beq", "bne", "beql", "bnel", "blez", "bgtz", "blezl", "bgtzl",
        "bltz", "bgez", "bltzl", "bgezl", "bltzal", "bgezal", "bltzall", "bgezall", "j", "jal",
        "lb", "lh", "lwl", "lw", "lbu", "lhu", "lwr", "lwu", "sb", "sh", "swl", "sw", "swr",
        "sdl", "sdr", "cache", "pref", "ll", "lld", "sc", "scd", "ld", "sd", "lq", "sq",
        "lwc1", "swc1", "ldc1", "sdc1", "lwc2", "swc2", "lqc2", "sqc2",
        "mfc0", "mtc0", "mfbpc", "mfiab", "mfiabm", "mfdab", "mfdabm",
        "mfdvb", "mfdvbm", "mfpc", "mfps", "mtbpc", "mtiab", "mtiabm",
        "mtdab", "mtdabm", "mtdvb", "mtdvbm", "mtpc", "mtps",
        "bc0f", "bc0t", "bc0fl", "bc0tl",
        "tlbr", "tlbwi", "tlbwr", "tlbp", "eret", "ei", "di",
        "mfc1", "cfc1", "mtc1", "ctc1", "bc1f", "bc1t", "bc1fl", "bc1tl",
        "qmfc2", "qmfc2.i", "qmfc2.ni", "qmtc2", "qmtc2.i", "qmtc2.ni",
        "cfc2", "cfc2.i", "cfc2.ni", "ctc2", "ctc2.i", "ctc2.ni",
        "bc2f", "bc2t", "bc2fl", "bc2tl",
        "vadd", "vsub", "vmul", "vmadd", "vmsub", "vmax", "vmini",
        "vaddx", "vaddy", "vaddz", "vaddw",
        "vsubx", "vsuby", "vsubz", "vsubw",
        "vmaddx", "vmaddy", "vmaddz", "vmaddw",
        "vmsubx", "vmsuby", "vmsubz", "vmsubw",
        "vmaxx", "vmaxy", "vmaxz", "vmaxw",
        "vminix", "vminiy", "vminiz", "vminiw",
        "vmulx", "vmuly", "vmulz", "vmulw",
        "add.s", "sub.s", "mul.s", "div.s", "sqrt.s", "abs.s", "mov.s", "neg.s",
        "rsqrt.s", "adda.s", "suba.s", "mula.s", "madd.s", "msub.s", "madda.s", "msuba.s",
        "max.s", "min.s", "cvt.w.s", "cvt.s.w", "c.f.s", "c.eq.s", "c.lt.s", "c.le.s",
        "paddw", "psubw", "pcgtw", "pmaxw", "paddh", "psubh", "pcgth", "pmaxh",
        "paddb", "psubb", "pcgtb", "paddsw", "psubsw", "pextlw", "ppacw", "paddsh",
        "psubsh", "pextlh", "ppach", "paddsb", "psubsb", "pextlb", "ppacb", "pext5",
        "ppac5", "pabsw", "pceqw", "pminw", "padsbh", "pabsh", "pceqh", "pminh",
        "pceqb", "padduw", "psubuw", "pextuw", "padduh", "psubuh", "pextuh", "paddub",
        "psubub", "pextub", "qfsrv", "pmaddw", "psllvw", "psrlvw", "pmsubw", "pmfhi",
        "pmflo", "pinth", "pmultw", "pdivw", "pcpyld", "pmaddh", "phmadh", "pand",
        "pxor", "pmsubh", "phmsbh", "pexeh", "prevh", "pmulth", "pdivbw", "pexew",
        "prot3w", "pmadduw", "psravw", "pmthi", "pmtlo", "pinteh", "pmultuw", "pdivuw",
        "pcpyud", "por", "pnor", "pexch", "pcpyh", "pexcw", "pmfhl.lw", "pmfhl.uw",
        "pmfhl.slw", "pmfhl.lh", "pmfhl.sh", "pmthl.lw", "psllh", "psrlh", "psrah", "psllw",
        "psrlw", "psraw"
    };
    return values;
}

bool is_known_mnemonic(std::string_view token) {
    const std::string mnemonic = lower_copy(token);
    if (known_mnemonics().find(mnemonic) != known_mnemonics().end()) return true;
    const std::size_t dot = mnemonic.find('.');
    return dot != std::string::npos && mnemonic.rfind('v', 0) == 0 &&
           known_mnemonics().find(mnemonic.substr(0, dot)) != known_mnemonics().end();
}

std::string normalize_label_key(std::string_view text) { return lower_copy(hex::trim(text)); }

bool try_parse_address(std::string_view token, std::uint32_t& address) {
    std::string clean = hex::trim(token);
    if (clean.size() >= 2U && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) clean.erase(0, 2U);
    if (clean.empty() || clean.size() > 8U) return false;
    const auto parsed = hex::parse_u32(clean);
    if (!parsed) return false;
    address = *parsed;
    return true;
}

bool try_parse_unsigned32(std::string_view token, std::uint32_t& value) {
    std::string clean = hex::trim(token);
    int base = 10;
    if (clean.size() >= 2U && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) {
        clean.erase(0, 2U);
        base = 16;
    } else if (!clean.empty() && (clean.back() == 'h' || clean.back() == 'H')) {
        clean.pop_back();
        base = 16;
    } else if (clean.size() == 8U || std::any_of(clean.begin(), clean.end(), [](char c) {
        return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    })) {
        base = 16;
    }
    if (clean.empty()) return false;
    try {
        std::size_t used = 0;
        const unsigned long long parsed = std::stoull(clean, &used, base);
        if (used != clean.size() || parsed > std::numeric_limits<std::uint32_t>::max()) return false;
        value = static_cast<std::uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_integer(std::string_view token,
                       const std::unordered_map<std::string, std::uint32_t>& labels,
                       std::int64_t& value) {
    std::string clean = hex::trim(token);
    const auto label = labels.find(normalize_label_key(clean));
    if (label != labels.end()) {
        value = label->second;
        return true;
    }

    const bool negative = !clean.empty() && clean.front() == '-';
    std::string body = negative ? clean.substr(1U) : clean;
    int base = 10;
    if (body.size() >= 2U && body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) {
        body.erase(0, 2U);
        base = 16;
    } else if (!body.empty() && (body.back() == 'h' || body.back() == 'H')) {
        body.pop_back();
        base = 16;
    } else if (body.size() == 8U || std::any_of(body.begin(), body.end(), [](char c) {
        return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    })) {
        base = 16;
    }
    if (body.empty()) return false;
    try {
        std::size_t used = 0;
        const unsigned long long parsed = std::stoull(body, &used, base);
        if (used != body.size() || parsed > static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max())) return false;
        value = negative ? -static_cast<std::int64_t>(parsed) : static_cast<std::int64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_register(std::string_view token, int& reg) {
    const auto found = register_map().find(lower_copy(hex::trim(token)));
    if (found == register_map().end()) return false;
    reg = found->second;
    return true;
}

bool try_parse_numbered_register(std::string_view token, std::string_view prefix1,
                                 std::string_view prefix2, int& reg) {
    std::string clean = lower_copy(hex::trim(token));
    if (!clean.empty() && clean.front() == '$') clean.erase(clean.begin());
    if (clean.rfind(prefix1, 0) == 0) clean.erase(0, prefix1.size());
    else if (!prefix2.empty() && clean.rfind(prefix2, 0) == 0) clean.erase(0, prefix2.size());
    else return false;
    try {
        std::size_t used = 0;
        const int parsed = std::stoi(clean, &used, 10);
        if (used != clean.size() || parsed < 0 || parsed > 31) return false;
        reg = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_fpu_register(std::string_view token, int& reg) {
    return try_parse_numbered_register(token, "f", {}, reg);
}

bool try_parse_cop2_register(std::string_view token, int& reg) {
    return try_parse_numbered_register(token, "vf", "c2r", reg);
}

bool try_parse_vu_integer_register(std::string_view token, int& reg) {
    return try_parse_numbered_register(token, "vi", "c2c", reg);
}

bool try_parse_cop_register(std::string_view token, int& reg) {
    return try_parse_numbered_register(token, "c", {}, reg);
}

bool try_parse_memory_operand(std::string_view token,
                              const std::unordered_map<std::string, std::uint32_t>& labels,
                              std::uint16_t& offset, int& base_register) {
    static const std::regex pattern(R"(^\s*([^()]*)\(([^()]+)\)\s*$)");
    std::smatch match;
    const std::string clean(token);
    if (!std::regex_match(clean, match, pattern) || !try_parse_register(match[2].str(), base_register)) return false;
    std::string offset_text = hex::trim(match[1].str());
    if (offset_text.empty()) offset_text = "0";
    std::int64_t value = 0;
    if (!try_parse_integer(offset_text, labels, value) || value < std::numeric_limits<std::int16_t>::min() || value > std::numeric_limits<std::uint16_t>::max()) return false;
    offset = static_cast<std::uint16_t>(value);
    return true;
}

bool try_get_numeric_jump_target(std::string_view instruction, std::uint32_t& target) {
    const std::string mnemonic = lower_copy(first_token(instruction));
    if (mnemonic != "j" && mnemonic != "jal") return false;
    const std::string rest = hex::trim(instruction.substr(first_token(instruction).size()));
    return try_parse_address(rest, target);
}

void append_direct_write(Cheat& cheat, int size, std::uint32_t address, std::uint32_t value) {
    cheat.append_pair((static_cast<std::uint32_t>(size) << 28U) | (address & 0x0FFFFFFFU), value);
}

bool try_get_directive(std::string_view line, ItemKind& kind, std::vector<std::string>& values) {
    static const std::regex pattern(R"(^\.(word|half|byte)\s+(.+)$)", std::regex::icase);
    std::smatch match;
    const std::string clean(line);
    if (!std::regex_match(clean, match, pattern)) return false;
    const std::string name = lower_copy(match[1].str());
    kind = name == "word" ? ItemKind::word : (name == "half" ? ItemKind::half : ItemKind::byte);
    std::string rest = match[2].str();
    std::size_t start = 0;
    while (start <= rest.size()) {
        const std::size_t comma = rest.find(',', start);
        const std::string value = hex::trim(std::string_view(rest).substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!value.empty()) values.push_back(value);
        if (comma == std::string::npos) break;
        start = comma + 1U;
    }
    return !values.empty();
}

std::vector<SourceBlock> parse_source_blocks(std::string_view input, std::vector<std::string>& errors) {
    std::vector<SourceBlock> blocks;
    std::optional<std::size_t> current_index;
    std::optional<std::uint32_t> current_address;
    bool hook_pending = false;
    std::optional<std::string> pending_credits;

    auto ensure_block = [&]() -> SourceBlock& {
        if (!current_index) {
            blocks.push_back(SourceBlock{});
            current_index = blocks.size() - 1U;
        }
        return blocks[*current_index];
    };

    auto start_named_block = [&](std::string name) {
        SourceBlock block;
        name = hex::trim(name);
        if (!name.empty()) block.name = name;
        if (pending_credits && block.name) {
            *block.name += " by " + *pending_credits;
            pending_credits.reset();
        }
        blocks.push_back(std::move(block));
        current_index = blocks.size() - 1U;
        current_address.reset();
        hook_pending = false;
    };

    const std::string normalized = newlines::to_lf(input);
    std::istringstream stream(normalized);
    std::string raw_line;
    int line_number = 0;
    static const std::regex hook_pattern(R"(^((?:0x)?[0-9A-Fa-f]{6,8})\s*:\s*Hook\s*$)", std::regex::icase);
    static const std::regex org_pattern(R"(^\.org\s+([^\s]+)\s*$)", std::regex::icase);
    static const std::regex code_pattern(R"(^\.code\s+([^\s,]+)[\s,]+([^\s,]+)\s*$)", std::regex::icase);
    static const std::regex addressed_pattern(R"(^((?:0x)?[0-9A-Fa-f]{6,8})\s*:\s*(.+)$)");
    static const std::regex label_pattern(R"(^([A-Za-z_.$][A-Za-z0-9_.$]*)\s*:\s*(.*)$)");

    while (std::getline(stream, raw_line)) {
        ++line_number;
        const std::string original = hex::trim(raw_line);
        if (original.empty()) continue;
        if (original.rfind("//", 0) == 0 || original.front() == '#' || original.front() == ';') continue;
        if (original.front() == '^') continue;
        if (original == "!!") { start_named_block({}); continue; }
        if (original.front() == '!' && original.rfind("!=", 0) != 0) {
            std::string group = hex::trim(std::string_view(original).substr(1U));
            if (!group.empty() && group.back() == ':') { group.pop_back(); group = hex::trim(group); }
            start_named_block(group);
            continue;
        }
        if (original.front() == '+') { start_named_block(hex::trim(std::string_view(original).substr(1U))); continue; }
        if (starts_with_ci(original, "%Credits:") || starts_with_ci(original, "%Credit:")) {
            const std::size_t colon = original.find(':');
            const std::string credits = colon == std::string::npos ? std::string{} : hex::trim(std::string_view(original).substr(colon + 1U));
            if (credits.empty()) continue;
            if (current_index && blocks[*current_index].name) {
                std::string name = *blocks[*current_index].name;
                const std::string lower = lower_copy(name);
                const std::size_t by = lower.find(" by ");
                if (by != std::string::npos) name.erase(by);
                blocks[*current_index].name = hex::trim(name) + " by " + credits;
            } else {
                pending_credits = credits;
            }
            continue;
        }

        std::string line = hex::trim(strip_inline_comment(original));
        if (line.empty()) continue;
        std::smatch match;
        if (std::regex_match(line, match, hook_pattern)) {
            std::uint32_t address = 0;
            if (!try_parse_address(match[1].str(), address)) {
                errors.push_back("Line " + std::to_string(line_number) + ": invalid Hook address.");
                continue;
            }
            (void)ensure_block();
            current_address = address;
            hook_pending = true;
            continue;
        }
        if (std::regex_match(line, match, org_pattern)) {
            std::uint32_t address = 0;
            if (!try_parse_address(match[1].str(), address)) {
                errors.push_back("Line " + std::to_string(line_number) + ": invalid .org address '" + match[1].str() + "'.");
                continue;
            }
            (void)ensure_block();
            current_address = address;
            hook_pending = false;
            continue;
        }
        if (std::regex_match(line, match, code_pattern)) {
            std::uint32_t address = 0, value = 0;
            if (!try_parse_unsigned32(match[1].str(), address) || !try_parse_unsigned32(match[2].str(), value)) {
                errors.push_back("Line " + std::to_string(line_number) + ": invalid .code address/value pair.");
                continue;
            }
            ensure_block().items.push_back(SourceItem{ItemKind::raw_code, line_number, 0U, {}, address, value});
            hook_pending = false;
            continue;
        }
        if (std::regex_match(line, match, addressed_pattern)) {
            std::uint32_t address = 0;
            if (!try_parse_address(match[1].str(), address)) {
                errors.push_back("Line " + std::to_string(line_number) + ": invalid address prefix.");
                continue;
            }
            (void)ensure_block();
            current_address = address;
            line = hex::trim(match[2].str());
            hook_pending = false;
        }
        if (std::regex_match(line, match, label_pattern)) {
            if (!current_address) {
                errors.push_back("Line " + std::to_string(line_number) + ": label '" + match[1].str() + "' needs an active .org/address.");
                continue;
            }
            SourceBlock& block = ensure_block();
            const std::string key = normalize_label_key(match[1].str());
            if (block.labels.find(key) != block.labels.end()) {
                errors.push_back("Line " + std::to_string(line_number) + ": duplicate label '" + match[1].str() + "'.");
                continue;
            }
            block.labels.emplace(key, *current_address);
            line = hex::trim(match[2].str());
            if (line.empty()) continue;
        }

        ItemKind directive_kind{};
        std::vector<std::string> directive_values;
        if (try_get_directive(line, directive_kind, directive_values)) {
            if (!current_address) {
                errors.push_back("Line " + std::to_string(line_number) + ": " + first_token(line) + " needs an active .org/address.");
                continue;
            }
            const std::uint32_t width = directive_kind == ItemKind::word ? 4U : (directive_kind == ItemKind::half ? 2U : 1U);
            SourceBlock& block = ensure_block();
            for (const std::string& value : directive_values) {
                block.items.push_back(SourceItem{directive_kind, line_number, *current_address, value, 0U, 0U});
                *current_address += width;
            }
            hook_pending = false;
            continue;
        }

        const std::string mnemonic = lower_copy(first_token(line));
        if (is_known_mnemonic(mnemonic)) {
            if (!current_address) {
                errors.push_back("Line " + std::to_string(line_number) + ": instruction '" + mnemonic + "' needs an active .org/address.");
                continue;
            }
            const std::uint32_t instruction_address = *current_address;
            ensure_block().items.push_back(SourceItem{ItemKind::instruction, line_number, instruction_address, line, 0U, 0U});
            std::uint32_t target = 0;
            if (hook_pending && try_get_numeric_jump_target(line, target)) current_address = target;
            else current_address = instruction_address + 4U;
            hook_pending = false;
            continue;
        }

        start_named_block(original);
    }
    return blocks;
}

std::uint32_t make_r(int rs, int rt, int rd, int shamt, int funct) {
    return (static_cast<std::uint32_t>(rs) << 21U) |
           (static_cast<std::uint32_t>(rt) << 16U) |
           (static_cast<std::uint32_t>(rd) << 11U) |
           (static_cast<std::uint32_t>(shamt) << 6U) |
           static_cast<std::uint32_t>(funct);
}

std::uint32_t make_i(int op, int rs, int rt, std::uint16_t imm) {
    return (static_cast<std::uint32_t>(op) << 26U) |
           (static_cast<std::uint32_t>(rs) << 21U) |
           (static_cast<std::uint32_t>(rt) << 16U) | imm;
}

std::uint32_t make_special2(int rs, int rt, int rd, int shamt, int funct) {
    return (0x1CU << 26U) | make_r(rs, rt, rd, shamt, funct);
}

enum class MmiOperandForm {
    d_s_t,
    d_t_s,
    d_t,
    s_t,
    d,
    s
};

struct MmiInstruction {
    std::string_view mnemonic;
    std::uint8_t funct;
    std::uint8_t subfunction;
    MmiOperandForm operands;
};

constexpr std::array<MmiInstruction, 78> mmi_instructions{{
    {"paddw", 0x08, 0x00, MmiOperandForm::d_s_t},
    {"psubw", 0x08, 0x01, MmiOperandForm::d_s_t},
    {"pcgtw", 0x08, 0x02, MmiOperandForm::d_s_t},
    {"pmaxw", 0x08, 0x03, MmiOperandForm::d_s_t},
    {"paddh", 0x08, 0x04, MmiOperandForm::d_s_t},
    {"psubh", 0x08, 0x05, MmiOperandForm::d_s_t},
    {"pcgth", 0x08, 0x06, MmiOperandForm::d_s_t},
    {"pmaxh", 0x08, 0x07, MmiOperandForm::d_s_t},
    {"paddb", 0x08, 0x08, MmiOperandForm::d_s_t},
    {"psubb", 0x08, 0x09, MmiOperandForm::d_s_t},
    {"pcgtb", 0x08, 0x0A, MmiOperandForm::d_s_t},
    {"paddsw", 0x08, 0x10, MmiOperandForm::d_s_t},
    {"psubsw", 0x08, 0x11, MmiOperandForm::d_s_t},
    {"pextlw", 0x08, 0x12, MmiOperandForm::d_s_t},
    {"ppacw", 0x08, 0x13, MmiOperandForm::d_s_t},
    {"paddsh", 0x08, 0x14, MmiOperandForm::d_s_t},
    {"psubsh", 0x08, 0x15, MmiOperandForm::d_s_t},
    {"pextlh", 0x08, 0x16, MmiOperandForm::d_s_t},
    {"ppach", 0x08, 0x17, MmiOperandForm::d_s_t},
    {"paddsb", 0x08, 0x18, MmiOperandForm::d_s_t},
    {"psubsb", 0x08, 0x19, MmiOperandForm::d_s_t},
    {"pextlb", 0x08, 0x1A, MmiOperandForm::d_s_t},
    {"ppacb", 0x08, 0x1B, MmiOperandForm::d_s_t},
    {"pext5", 0x08, 0x1E, MmiOperandForm::d_t},
    {"ppac5", 0x08, 0x1F, MmiOperandForm::d_t},
    {"pabsw", 0x28, 0x01, MmiOperandForm::d_t},
    {"pceqw", 0x28, 0x02, MmiOperandForm::d_s_t},
    {"pminw", 0x28, 0x03, MmiOperandForm::d_s_t},
    {"padsbh", 0x28, 0x04, MmiOperandForm::d_s_t},
    {"pabsh", 0x28, 0x05, MmiOperandForm::d_t},
    {"pceqh", 0x28, 0x06, MmiOperandForm::d_s_t},
    {"pminh", 0x28, 0x07, MmiOperandForm::d_s_t},
    {"pceqb", 0x28, 0x0A, MmiOperandForm::d_s_t},
    {"padduw", 0x28, 0x10, MmiOperandForm::d_s_t},
    {"psubuw", 0x28, 0x11, MmiOperandForm::d_s_t},
    {"pextuw", 0x28, 0x12, MmiOperandForm::d_s_t},
    {"padduh", 0x28, 0x14, MmiOperandForm::d_s_t},
    {"psubuh", 0x28, 0x15, MmiOperandForm::d_s_t},
    {"pextuh", 0x28, 0x16, MmiOperandForm::d_s_t},
    {"paddub", 0x28, 0x18, MmiOperandForm::d_s_t},
    {"psubub", 0x28, 0x19, MmiOperandForm::d_s_t},
    {"pextub", 0x28, 0x1A, MmiOperandForm::d_s_t},
    {"qfsrv", 0x28, 0x1B, MmiOperandForm::d_s_t},
    {"pmaddw", 0x09, 0x00, MmiOperandForm::d_s_t},
    {"psllvw", 0x09, 0x02, MmiOperandForm::d_t_s},
    {"psrlvw", 0x09, 0x03, MmiOperandForm::d_t_s},
    {"pmsubw", 0x09, 0x04, MmiOperandForm::d_s_t},
    {"pmfhi", 0x09, 0x08, MmiOperandForm::d},
    {"pmflo", 0x09, 0x09, MmiOperandForm::d},
    {"pinth", 0x09, 0x0A, MmiOperandForm::d_s_t},
    {"pmultw", 0x09, 0x0C, MmiOperandForm::d_s_t},
    {"pdivw", 0x09, 0x0D, MmiOperandForm::s_t},
    {"pcpyld", 0x09, 0x0E, MmiOperandForm::d_s_t},
    {"pmaddh", 0x09, 0x10, MmiOperandForm::d_s_t},
    {"phmadh", 0x09, 0x11, MmiOperandForm::d_s_t},
    {"pand", 0x09, 0x12, MmiOperandForm::d_s_t},
    {"pxor", 0x09, 0x13, MmiOperandForm::d_s_t},
    {"pmsubh", 0x09, 0x14, MmiOperandForm::d_s_t},
    {"phmsbh", 0x09, 0x15, MmiOperandForm::d_s_t},
    {"pexeh", 0x09, 0x1A, MmiOperandForm::d_t},
    {"prevh", 0x09, 0x1B, MmiOperandForm::d_t},
    {"pmulth", 0x09, 0x1C, MmiOperandForm::d_s_t},
    {"pdivbw", 0x09, 0x1D, MmiOperandForm::s_t},
    {"pexew", 0x09, 0x1E, MmiOperandForm::d_t},
    {"prot3w", 0x09, 0x1F, MmiOperandForm::d_t},
    {"pmadduw", 0x29, 0x00, MmiOperandForm::d_s_t},
    {"psravw", 0x29, 0x03, MmiOperandForm::d_t_s},
    {"pmthi", 0x29, 0x08, MmiOperandForm::s},
    {"pmtlo", 0x29, 0x09, MmiOperandForm::s},
    {"pinteh", 0x29, 0x0A, MmiOperandForm::d_s_t},
    {"pmultuw", 0x29, 0x0C, MmiOperandForm::d_s_t},
    {"pdivuw", 0x29, 0x0D, MmiOperandForm::s_t},
    {"pcpyud", 0x29, 0x0E, MmiOperandForm::d_s_t},
    {"por", 0x29, 0x12, MmiOperandForm::d_s_t},
    {"pnor", 0x29, 0x13, MmiOperandForm::d_s_t},
    {"pexch", 0x29, 0x1A, MmiOperandForm::d_t},
    {"pcpyh", 0x29, 0x1B, MmiOperandForm::d_t},
    {"pexcw", 0x29, 0x1E, MmiOperandForm::d_t},
}};

const MmiInstruction* find_mmi_instruction(std::string_view mnemonic) {
    const auto found = std::find_if(mmi_instructions.begin(), mmi_instructions.end(),
        [mnemonic](const MmiInstruction& instruction) {
            return instruction.mnemonic == mnemonic;
        });
    return found == mmi_instructions.end() ? nullptr : &*found;
}

const MmiInstruction* decode_mmi_instruction(int funct, int subfunction) {
    const auto found = std::find_if(mmi_instructions.begin(), mmi_instructions.end(),
        [funct, subfunction](const MmiInstruction& instruction) {
            return instruction.funct == funct && instruction.subfunction == subfunction;
        });
    return found == mmi_instructions.end() ? nullptr : &*found;
}

struct VuMacroInstruction {
    std::string_view mnemonic;
    std::uint8_t funct;
};

constexpr std::array<VuMacroInstruction, 35> vu_macro_instructions{{
    {"vaddx", 0x00}, {"vaddy", 0x01}, {"vaddz", 0x02}, {"vaddw", 0x03},
    {"vsubx", 0x04}, {"vsuby", 0x05}, {"vsubz", 0x06}, {"vsubw", 0x07},
    {"vmaddx", 0x08}, {"vmaddy", 0x09}, {"vmaddz", 0x0A}, {"vmaddw", 0x0B},
    {"vmsubx", 0x0C}, {"vmsuby", 0x0D}, {"vmsubz", 0x0E}, {"vmsubw", 0x0F},
    {"vmaxx", 0x10}, {"vmaxy", 0x11}, {"vmaxz", 0x12}, {"vmaxw", 0x13},
    {"vminix", 0x14}, {"vminiy", 0x15}, {"vminiz", 0x16}, {"vminiw", 0x17},
    {"vmulx", 0x18}, {"vmuly", 0x19}, {"vmulz", 0x1A}, {"vmulw", 0x1B},
    {"vadd", 0x28}, {"vmadd", 0x29}, {"vmul", 0x2A}, {"vmax", 0x2B},
    {"vsub", 0x2C}, {"vmsub", 0x2D}, {"vmini", 0x2F},
}};

const VuMacroInstruction* find_vu_macro_instruction(std::string_view mnemonic) {
    const auto found = std::find_if(vu_macro_instructions.begin(), vu_macro_instructions.end(),
        [mnemonic](const VuMacroInstruction& instruction) {
            return instruction.mnemonic == mnemonic;
        });
    return found == vu_macro_instructions.end() ? nullptr : &*found;
}

const VuMacroInstruction* decode_vu_macro_instruction(int funct) {
    const auto found = std::find_if(vu_macro_instructions.begin(), vu_macro_instructions.end(),
        [funct](const VuMacroInstruction& instruction) {
            return instruction.funct == funct;
        });
    return found == vu_macro_instructions.end() ? nullptr : &*found;
}

bool parse_vu_mnemonic(std::string_view token, std::string& base, int& destination_mask,
                       std::string& error) {
    const std::string clean = lower_copy(token);
    const std::size_t dot = clean.find('.');
    base = dot == std::string::npos ? clean : clean.substr(0, dot);
    if (find_vu_macro_instruction(base) == nullptr) return false;

    destination_mask = 0xF;
    if (dot == std::string::npos) return true;
    const std::string suffix = clean.substr(dot + 1U);
    if (suffix.empty()) {
        error = "VU destination suffix after '.' cannot be empty.";
        return true;
    }
    destination_mask = 0;
    for (const char component : suffix) {
        int bit = 0;
        if (component == 'x') bit = 8;
        else if (component == 'y') bit = 4;
        else if (component == 'z') bit = 2;
        else if (component == 'w') bit = 1;
        else {
            error = "invalid VU destination component '" + std::string(1, component) +
                    "'; expected x, y, z, and/or w.";
            return true;
        }
        if ((destination_mask & bit) != 0) {
            error = "duplicate VU destination component '" + std::string(1, component) + "'.";
            return true;
        }
        destination_mask |= bit;
    }
    return true;
}

std::string format_vu_destination_mask(int mask) {
    std::string result;
    if ((mask & 8) != 0) result += 'x';
    if ((mask & 4) != 0) result += 'y';
    if ((mask & 2) != 0) result += 'z';
    if ((mask & 1) != 0) result += 'w';
    return result;
}

class Assembler {
public:
    Assembler(std::string_view text, std::uint32_t pc,
              const std::unordered_map<std::string, std::uint32_t>& labels)
        : text_(text), pc_(pc), labels_(labels), mnemonic_(lower_copy(first_token(text))) {
        const std::string token = first_token(text);
        operands_ = split_operands(text.substr(token.size()));
    }

    bool run(std::uint32_t& word, std::string& error);

private:
    bool need(std::size_t count) {
        if (operands_.size() == count) return true;
        error_ = "'" + mnemonic_ + "' expects " + std::to_string(count) +
                 " operand(s), got " + std::to_string(operands_.size()) + ".";
        return false;
    }
    bool reg(std::size_t index, int& value) {
        if (index < operands_.size() && try_parse_register(operands_[index], value)) return true;
        error_ = index < operands_.size() ? "invalid register '" + operands_[index] + "'." : "missing register operand.";
        return false;
    }
    bool freg(std::size_t index, int& value) {
        if (index < operands_.size() && try_parse_fpu_register(operands_[index], value)) return true;
        error_ = index < operands_.size() ? "invalid FPU register '" + operands_[index] + "'." : "missing FPU register operand.";
        return false;
    }
    bool c2reg(std::size_t index, int& value) {
        if (index < operands_.size() && try_parse_cop2_register(operands_[index], value)) return true;
        error_ = index < operands_.size() ? "invalid COP2/VU floating register '" + operands_[index] + "'." : "missing COP2/VU floating register operand.";
        return false;
    }
    bool vireg(std::size_t index, int& value) {
        if (index < operands_.size() && try_parse_vu_integer_register(operands_[index], value)) return true;
        error_ = index < operands_.size() ? "invalid COP2/VU integer/control register '" + operands_[index] + "'." : "missing COP2/VU integer/control register operand.";
        return false;
    }
    bool imm(std::size_t index, std::int64_t& value) {
        if (index < operands_.size() && try_parse_integer(operands_[index], labels_, value)) return true;
        error_ = index < operands_.size() ? "invalid immediate/label '" + operands_[index] + "'." : "missing immediate operand.";
        return false;
    }
    bool signed16(std::size_t index, std::uint16_t& value) {
        std::int64_t parsed = 0;
        if (!imm(index, parsed)) return false;
        if (parsed < std::numeric_limits<std::int16_t>::min() || parsed > std::numeric_limits<std::uint16_t>::max()) {
            error_ = "immediate '" + operands_[index] + "' does not fit 16 bits.";
            return false;
        }
        value = static_cast<std::uint16_t>(parsed);
        return true;
    }
    bool unsigned16(std::size_t index, std::uint16_t& value) {
        std::int64_t parsed = 0;
        if (!imm(index, parsed)) return false;
        if (parsed < 0 || parsed > std::numeric_limits<std::uint16_t>::max()) {
            error_ = "immediate '" + operands_[index] + "' must be 0..0xFFFF.";
            return false;
        }
        value = static_cast<std::uint16_t>(parsed);
        return true;
    }
    bool branch_offset(std::size_t index, std::uint16_t& encoded) {
        std::int64_t target = 0;
        if (!imm(index, target)) return false;
        const std::int64_t delta = target - (static_cast<std::int64_t>(pc_) + 4);
        if ((delta & 3) != 0) {
            std::ostringstream out; out << "branch target 0x" << std::uppercase << std::hex << target << " is not word-aligned.";
            error_ = out.str(); return false;
        }
        const std::int64_t offset = delta >> 2;
        if (offset < std::numeric_limits<std::int16_t>::min() || offset > std::numeric_limits<std::int16_t>::max()) {
            std::ostringstream out; out << "branch target 0x" << std::uppercase << std::hex << target << " is out of 16-bit range.";
            error_ = out.str(); return false;
        }
        encoded = static_cast<std::uint16_t>(static_cast<std::int16_t>(offset));
        return true;
    }
    bool jump_target(std::size_t index, std::uint32_t& encoded) {
        std::int64_t target = 0;
        if (!imm(index, target)) return false;
        if ((target & 3) != 0 || target < 0 || target > std::numeric_limits<std::uint32_t>::max()) {
            error_ = "jump target '" + operands_[index] + "' is invalid or unaligned.";
            return false;
        }
        const auto target32 = static_cast<std::uint32_t>(target);
        if (((pc_ + 4U) & 0xF0000000U) != (target32 & 0xF0000000U)) {
            error_ = "jump target 0x" + format_hex(target32, 8) + " is outside the current 256 MiB region.";
            return false;
        }
        encoded = (target32 >> 2U) & 0x03FFFFFFU;
        return true;
    }
    bool memory(std::size_t index, std::uint16_t& offset, int& base) {
        if (index < operands_.size() && try_parse_memory_operand(operands_[index], labels_, offset, base)) return true;
        error_ = index < operands_.size() ? "invalid memory operand '" + operands_[index] + "'; expected offset(base)." : "missing memory operand.";
        return false;
    }

    std::string text_;
    std::uint32_t pc_{};
    const std::unordered_map<std::string, std::uint32_t>& labels_;
    std::string mnemonic_;
    std::vector<std::string> operands_;
    std::string error_;
};

bool Assembler::run(std::uint32_t& word, std::string& error) {
    word = 0U;
    int rd = 0, rs = 0, rt = 0, rs2 = 0, fs = 0, fd = 0, ft = 0;
    std::uint16_t immediate = 0, branch = 0, offset = 0;
    std::uint32_t target = 0;
    std::int64_t number = 0;

    if (mnemonic_ == "nop") {
        if (!need(0)) goto fail;
        word = 0U; return true;
    }
    if (mnemonic_ == "move") {
        if (!need(2) || !reg(0, rd) || !reg(1, rs)) goto fail;
        word = make_r(rs, 0, rd, 0, 0x21); return true;
    }
    if (mnemonic_ == "clear") {
        if (!need(1) || !reg(0, rd)) goto fail;
        word = make_r(0, 0, rd, 0, 0x21); return true;
    }
    if (mnemonic_ == "negu") {
        if (!need(2) || !reg(0, rd) || !reg(1, rs)) goto fail;
        word = make_r(0, rs, rd, 0, 0x23); return true;
    }
    if (mnemonic_ == "not") {
        if (!need(2) || !reg(0, rd) || !reg(1, rs)) goto fail;
        word = make_r(rs, 0, rd, 0, 0x27); return true;
    }
    if (mnemonic_ == "li") {
        if (!need(2) || !reg(0, rt) || !imm(1, number)) goto fail;
        if (number >= std::numeric_limits<std::int16_t>::min() && number <= std::numeric_limits<std::int16_t>::max()) {
            word = make_i(0x09, 0, rt, static_cast<std::uint16_t>(static_cast<std::int16_t>(number)));
            return true;
        }
        if (number >= 0 && number <= std::numeric_limits<std::uint16_t>::max()) {
            word = make_i(0x0D, 0, rt, static_cast<std::uint16_t>(number));
            return true;
        }
        error_ = "'li' only supports a single 16-bit instruction; use lui/ori for larger values.";
        goto fail;
    }
    if (mnemonic_ == "b") {
        if (!need(1) || !branch_offset(0, branch)) goto fail;
        word = make_i(0x04, 0, 0, branch); return true;
    }
    if (mnemonic_ == "bal") {
        if (!need(1) || !branch_offset(0, branch)) goto fail;
        word = make_i(0x01, 0, 0x11, branch); return true;
    }
    if (mnemonic_ == "beqz") {
        if (!need(2) || !reg(0, rs) || !branch_offset(1, branch)) goto fail;
        word = make_i(0x04, rs, 0, branch); return true;
    }
    if (mnemonic_ == "bnez") {
        if (!need(2) || !reg(0, rs) || !branch_offset(1, branch)) goto fail;
        word = make_i(0x05, rs, 0, branch); return true;
    }

    {
        static const std::unordered_map<std::string, int> operations{
            {"add", 0x20}, {"addu", 0x21}, {"sub", 0x22}, {"subu", 0x23},
            {"and", 0x24}, {"or", 0x25}, {"xor", 0x26}, {"nor", 0x27},
            {"slt", 0x2A}, {"sltu", 0x2B}, {"dadd", 0x2C}, {"daddu", 0x2D},
            {"dsub", 0x2E}, {"dsubu", 0x2F}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !reg(0, rd) || !reg(1, rs) || !reg(2, rt)) goto fail;
            word = make_r(rs, rt, rd, 0, found->second); return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"tge", 0x30}, {"tgeu", 0x31}, {"tlt", 0x32},
            {"tltu", 0x33}, {"teq", 0x34}, {"tne", 0x36}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (operands_.size() < 2U || operands_.size() > 3U) {
                error_ = "'" + mnemonic_ + "' expects 2 or 3 operands.";
                goto fail;
            }
            if (!reg(0, rs) || !reg(1, rt)) goto fail;
            number = 0;
            if (operands_.size() == 3U && !imm(2, number)) goto fail;
            if (number < 0 || number > 0x3FF) {
                error_ = "trap code for '" + mnemonic_ + "' must be 0..0x3FF.";
                goto fail;
            }
            const int code = static_cast<int>(number);
            word = make_r(rs, rt, (code >> 5) & 31, code & 31, found->second);
            return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"sll", 0x00}, {"srl", 0x02}, {"sra", 0x03},
            {"dsll", 0x38}, {"dsrl", 0x3A}, {"dsra", 0x3B},
            {"dsll32", 0x3C}, {"dsrl32", 0x3E}, {"dsra32", 0x3F}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !reg(0, rd) || !reg(1, rt) || !imm(2, number)) goto fail;
            if (number < 0 || number > 31) { error_ = "shift amount must be 0..31."; goto fail; }
            word = make_r(0, rt, rd, static_cast<int>(number), found->second); return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"sllv", 0x04}, {"srlv", 0x06}, {"srav", 0x07},
            {"dsllv", 0x14}, {"dsrlv", 0x16}, {"dsrav", 0x17}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !reg(0, rd) || !reg(1, rt) || !reg(2, rs)) goto fail;
            word = make_r(rs, rt, rd, 0, found->second); return true;
        }
    }
    if (mnemonic_ == "jr") {
        if (!need(1) || !reg(0, rs)) goto fail;
        word = make_r(rs, 0, 0, 0, 0x08); return true;
    }
    if (mnemonic_ == "jalr") {
        if (operands_.size() == 1U) {
            if (!reg(0, rs)) goto fail;
            word = make_r(rs, 0, 31, 0, 0x09); return true;
        }
        if (!need(2) || !reg(0, rd) || !reg(1, rs2)) goto fail;
        word = make_r(rs2, 0, rd, 0, 0x09); return true;
    }
    if (mnemonic_ == "movz" || mnemonic_ == "movn") {
        if (!need(3) || !reg(0, rd) || !reg(1, rs) || !reg(2, rt)) goto fail;
        word = make_r(rs, rt, rd, 0, mnemonic_ == "movz" ? 0x0A : 0x0B); return true;
    }
    if (mnemonic_ == "syscall" || mnemonic_ == "break") {
        if (operands_.size() > 1U) { error_ = "'" + mnemonic_ + "' accepts zero or one code operand."; goto fail; }
        number = 0;
        if (operands_.size() == 1U && !imm(0, number)) goto fail;
        if (number < 0 || number > 0xFFFFF) { error_ = "'" + mnemonic_ + "' code must fit 20 bits."; goto fail; }
        word = (static_cast<std::uint32_t>(number) << 6U) | static_cast<std::uint32_t>(mnemonic_ == "syscall" ? 0x0C : 0x0D);
        return true;
    }
    if (mnemonic_ == "sync") {
        if (operands_.size() > 1U) { error_ = "'sync' accepts zero or one stype operand."; goto fail; }
        number = 0;
        if (operands_.size() == 1U && !imm(0, number)) goto fail;
        if (number < 0 || number > 31) { error_ = "'sync' stype must be 0..31."; goto fail; }
        word = make_r(0, 0, 0, static_cast<int>(number), 0x0F); return true;
    }
    if (mnemonic_ == "mfhi" || mnemonic_ == "mflo") {
        if (!need(1) || !reg(0, rd)) goto fail;
        word = make_r(0, 0, rd, 0, mnemonic_ == "mfhi" ? 0x10 : 0x12); return true;
    }
    if (mnemonic_ == "mthi" || mnemonic_ == "mtlo") {
        if (!need(1) || !reg(0, rs)) goto fail;
        word = make_r(rs, 0, 0, 0, mnemonic_ == "mthi" ? 0x11 : 0x13); return true;
    }
    if (mnemonic_ == "mfsa") {
        if (!need(1) || !reg(0, rd)) goto fail;
        word = make_r(0, 0, rd, 0, 0x28); return true;
    }
    if (mnemonic_ == "mtsa") {
        if (!need(1) || !reg(0, rs)) goto fail;
        word = make_r(rs, 0, 0, 0, 0x29); return true;
    }
    if (mnemonic_ == "mult" || mnemonic_ == "multu") {
        const int funct = mnemonic_ == "mult" ? 0x18 : 0x19;
        if (operands_.size() == 2U) {
            if (!reg(0, rs) || !reg(1, rt)) goto fail;
            word = make_r(rs, rt, 0, 0, funct); return true;
        }
        if (!need(3) || !reg(0, rd) || !reg(1, rs) || !reg(2, rt)) goto fail;
        word = make_r(rs, rt, rd, 0, funct); return true;
    }
    if (mnemonic_ == "div" || mnemonic_ == "divu") {
        if (!need(2) || !reg(0, rs) || !reg(1, rt)) goto fail;
        word = make_r(rs, rt, 0, 0, mnemonic_ == "div" ? 0x1A : 0x1B); return true;
    }
    if (mnemonic_ == "mtsab" || mnemonic_ == "mtsah") {
        if (!need(2) || !reg(0, rs) || !signed16(1, immediate)) goto fail;
        word = make_i(0x01, rs, mnemonic_ == "mtsab" ? 0x18 : 0x19, immediate); return true;
    }
    if (mnemonic_ == "mfhi1" || mnemonic_ == "mflo1") {
        if (!need(1) || !reg(0, rd)) goto fail;
        word = make_special2(0, 0, rd, 0, mnemonic_ == "mfhi1" ? 0x10 : 0x12); return true;
    }
    if (mnemonic_ == "mthi1" || mnemonic_ == "mtlo1") {
        if (!need(1) || !reg(0, rs)) goto fail;
        word = make_special2(rs, 0, 0, 0, mnemonic_ == "mthi1" ? 0x11 : 0x13); return true;
    }
    if (mnemonic_ == "div1" || mnemonic_ == "divu1") {
        if (!need(2) || !reg(0, rs) || !reg(1, rt)) goto fail;
        word = make_special2(rs, rt, 0, 0, mnemonic_ == "div1" ? 0x1A : 0x1B); return true;
    }
    if (mnemonic_ == "mult1" || mnemonic_ == "multu1") {
        const int funct = mnemonic_ == "mult1" ? 0x18 : 0x19;
        if (operands_.size() == 2U) {
            if (!reg(0, rs) || !reg(1, rt)) goto fail;
            word = make_special2(rs, rt, 0, 0, funct); return true;
        }
        if (!need(3) || !reg(0, rd) || !reg(1, rs) || !reg(2, rt)) goto fail;
        word = make_special2(rs, rt, rd, 0, funct); return true;
    }
    if (mnemonic_ == "madd" || mnemonic_ == "maddu" ||
        mnemonic_ == "madd1" || mnemonic_ == "maddu1") {
        int funct = 0;
        if (mnemonic_ == "maddu") funct = 0x01;
        else if (mnemonic_ == "madd1") funct = 0x20;
        else if (mnemonic_ == "maddu1") funct = 0x21;
        if (operands_.size() == 2U) {
            if (!reg(0, rs) || !reg(1, rt)) goto fail;
            word = make_special2(rs, rt, 0, 0, funct); return true;
        }
        if (!need(3) || !reg(0, rd) || !reg(1, rs) || !reg(2, rt)) goto fail;
        word = make_special2(rs, rt, rd, 0, funct); return true;
    }
    if (mnemonic_ == "plzcw") {
        if (!need(2) || !reg(0, rd) || !reg(1, rs)) goto fail;
        word = make_special2(rs, 0, rd, 0, 0x04); return true;
    }
    if (const MmiInstruction* instruction = find_mmi_instruction(mnemonic_)) {
        switch (instruction->operands) {
            case MmiOperandForm::d_s_t:
                if (!need(3) || !reg(0, rd) || !reg(1, rs) || !reg(2, rt)) goto fail;
                break;
            case MmiOperandForm::d_t_s:
                if (!need(3) || !reg(0, rd) || !reg(1, rt) || !reg(2, rs)) goto fail;
                break;
            case MmiOperandForm::d_t:
                if (!need(2) || !reg(0, rd) || !reg(1, rt)) goto fail;
                rs = 0;
                break;
            case MmiOperandForm::s_t:
                if (!need(2) || !reg(0, rs) || !reg(1, rt)) goto fail;
                rd = 0;
                break;
            case MmiOperandForm::d:
                if (!need(1) || !reg(0, rd)) goto fail;
                rs = 0;
                rt = 0;
                break;
            case MmiOperandForm::s:
                if (!need(1) || !reg(0, rs)) goto fail;
                rt = 0;
                rd = 0;
                break;
        }
        word = make_special2(rs, rt, rd, instruction->subfunction, instruction->funct);
        return true;
    }
    if (mnemonic_ == "pmfhl.lw" || mnemonic_ == "pmfhl.uw" ||
        mnemonic_ == "pmfhl.slw" || mnemonic_ == "pmfhl.lh" ||
        mnemonic_ == "pmfhl.sh") {
        if (!need(1) || !reg(0, rd)) goto fail;
        int mode = 0;
        if (mnemonic_ == "pmfhl.uw") mode = 1;
        else if (mnemonic_ == "pmfhl.slw") mode = 2;
        else if (mnemonic_ == "pmfhl.lh") mode = 3;
        else if (mnemonic_ == "pmfhl.sh") mode = 4;
        word = make_special2(0, 0, rd, mode, 0x30);
        return true;
    }
    if (mnemonic_ == "pmthl.lw") {
        if (!need(1) || !reg(0, rs)) goto fail;
        word = make_special2(rs, 0, 0, 0, 0x31);
        return true;
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"psllh", 0x34}, {"psrlh", 0x36}, {"psrah", 0x37},
            {"psllw", 0x3C}, {"psrlw", 0x3E}, {"psraw", 0x3F}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !reg(0, rd) || !reg(1, rt) || !imm(2, number)) goto fail;
            const bool halfword_shift = mnemonic_ == "psllh" || mnemonic_ == "psrlh" || mnemonic_ == "psrah";
            const int maximum = halfword_shift ? 15 : 31;
            if (number < 0 || number > maximum) {
                error_ = "packed shift amount for '" + mnemonic_ + "' must be 0.." +
                         std::to_string(maximum) + ".";
                goto fail;
            }
            word = make_special2(0, rt, rd, static_cast<int>(number), found->second);
            return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"addi", 0x08}, {"addiu", 0x09}, {"slti", 0x0A}, {"sltiu", 0x0B},
            {"daddi", 0x18}, {"daddiu", 0x19}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !reg(0, rt) || !reg(1, rs) || !signed16(2, immediate)) goto fail;
            word = make_i(found->second, rs, rt, immediate); return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"tgei", 0x08}, {"tgeiu", 0x09}, {"tlti", 0x0A},
            {"tltiu", 0x0B}, {"teqi", 0x0C}, {"tnei", 0x0E}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(2) || !reg(0, rs) || !signed16(1, immediate)) goto fail;
            word = make_i(0x01, rs, found->second, immediate);
            return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"andi", 0x0C}, {"ori", 0x0D}, {"xori", 0x0E}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !reg(0, rt) || !reg(1, rs) || !unsigned16(2, immediate)) goto fail;
            word = make_i(found->second, rs, rt, immediate); return true;
        }
    }
    if (mnemonic_ == "lui") {
        if (!need(2) || !reg(0, rt) || !unsigned16(1, immediate)) goto fail;
        word = make_i(0x0F, 0, rt, immediate); return true;
    }
    if (mnemonic_ == "beq" || mnemonic_ == "bne" || mnemonic_ == "beql" || mnemonic_ == "bnel") {
        if (!need(3) || !reg(0, rs) || !reg(1, rt) || !branch_offset(2, branch)) goto fail;
        const int op = mnemonic_ == "beq" ? 0x04 :
                       mnemonic_ == "bne" ? 0x05 :
                       mnemonic_ == "beql" ? 0x14 : 0x15;
        word = make_i(op, rs, rt, branch); return true;
    }
    if (mnemonic_ == "blez" || mnemonic_ == "bgtz" || mnemonic_ == "blezl" || mnemonic_ == "bgtzl") {
        if (!need(2) || !reg(0, rs) || !branch_offset(1, branch)) goto fail;
        const int op = mnemonic_ == "blez" ? 0x06 :
                       mnemonic_ == "bgtz" ? 0x07 :
                       mnemonic_ == "blezl" ? 0x16 : 0x17;
        word = make_i(op, rs, 0, branch); return true;
    }
    if (mnemonic_ == "bltz" || mnemonic_ == "bgez" || mnemonic_ == "bltzl" || mnemonic_ == "bgezl" ||
        mnemonic_ == "bltzal" || mnemonic_ == "bgezal" || mnemonic_ == "bltzall" || mnemonic_ == "bgezall") {
        if (!need(2) || !reg(0, rs) || !branch_offset(1, branch)) goto fail;
        if (mnemonic_ == "bltz") rt = 0x00;
        else if (mnemonic_ == "bgez") rt = 0x01;
        else if (mnemonic_ == "bltzl") rt = 0x02;
        else if (mnemonic_ == "bgezl") rt = 0x03;
        else if (mnemonic_ == "bltzal") rt = 0x10;
        else if (mnemonic_ == "bgezal") rt = 0x11;
        else if (mnemonic_ == "bltzall") rt = 0x12;
        else rt = 0x13;
        word = make_i(0x01, rs, rt, branch); return true;
    }
    if (mnemonic_ == "j" || mnemonic_ == "jal") {
        if (!need(1) || !jump_target(0, target)) goto fail;
        word = (static_cast<std::uint32_t>(mnemonic_ == "j" ? 0x02 : 0x03) << 26U) | target;
        return true;
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"lb", 0x20}, {"lh", 0x21}, {"lwl", 0x22}, {"lw", 0x23},
            {"lbu", 0x24}, {"lhu", 0x25}, {"lwr", 0x26}, {"lwu", 0x27},
            {"sb", 0x28}, {"sh", 0x29}, {"swl", 0x2A}, {"sw", 0x2B},
            {"sdl", 0x2C}, {"sdr", 0x2D}, {"swr", 0x2E}, {"cache", 0x2F},
            {"lwc1", 0x31}, {"pref", 0x33}, {"lqc2", 0x36}, {"ld", 0x37},
            {"swc1", 0x39}, {"sqc2", 0x3E}, {"sd", 0x3F},
            {"ldl", 0x1A}, {"ldr", 0x1B}, {"lq", 0x1E}, {"sq", 0x1F}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            int base = 0;
            if (!need(2) || !memory(1, offset, base)) goto fail;
            if (mnemonic_ == "lwc1" || mnemonic_ == "swc1" || mnemonic_ == "ldc1" || mnemonic_ == "sdc1") {
                if (!freg(0, rt)) goto fail;
            } else if (mnemonic_ == "lwc2" || mnemonic_ == "swc2" || mnemonic_ == "lqc2" || mnemonic_ == "sqc2") {
                if (!c2reg(0, rt)) goto fail;
            } else if (mnemonic_ == "cache" || mnemonic_ == "pref") {
                if (!imm(0, number) || number < 0 || number > 31) {
                    error_ = "'" + mnemonic_ + "' operation/hint must be 0..31."; goto fail;
                }
                rt = static_cast<int>(number);
            } else if (!reg(0, rt)) {
                goto fail;
            }
            word = make_i(found->second, base, rt, offset); return true;
        }
    }
    if (mnemonic_ == "bc0f" || mnemonic_ == "bc0t" || mnemonic_ == "bc0fl" || mnemonic_ == "bc0tl") {
        if (!need(1) || !branch_offset(0, branch)) goto fail;
        rt = mnemonic_ == "bc0f" ? 0 :
             mnemonic_ == "bc0t" ? 1 :
             mnemonic_ == "bc0fl" ? 2 : 3;
        word = (0x10U << 26U) | (8U << 21U) | (static_cast<std::uint32_t>(rt) << 16U) | branch;
        return true;
    }
    if (mnemonic_ == "tlbr" || mnemonic_ == "tlbwi" || mnemonic_ == "tlbwr" || mnemonic_ == "tlbp" ||
        mnemonic_ == "eret" || mnemonic_ == "ei" || mnemonic_ == "di") {
        if (!need(0)) goto fail;
        if (mnemonic_ == "tlbr") word = 0x42000001U;
        else if (mnemonic_ == "tlbwi") word = 0x42000002U;
        else if (mnemonic_ == "tlbwr") word = 0x42000006U;
        else if (mnemonic_ == "tlbp") word = 0x42000008U;
        else if (mnemonic_ == "eret") word = 0x42000018U;
        else if (mnemonic_ == "ei") word = 0x42000038U;
        else word = 0x42000039U;
        return true;
    }
    {
        static const std::unordered_map<std::string, int> debug_register_codes{
            {"mfbpc", 0}, {"mfiab", 2}, {"mfiabm", 3}, {"mfdab", 4},
            {"mfdabm", 5}, {"mfdvb", 6}, {"mfdvbm", 7},
            {"mtbpc", 0}, {"mtiab", 2}, {"mtiabm", 3}, {"mtdab", 4},
            {"mtdabm", 5}, {"mtdvb", 6}, {"mtdvbm", 7}
        };
        const auto found = debug_register_codes.find(mnemonic_);
        if (found != debug_register_codes.end()) {
            if (!need(1) || !reg(0, rt)) goto fail;
            const bool move_to = !mnemonic_.empty() && mnemonic_[0] == 'm' && mnemonic_[1] == 't';
            rs = move_to ? 4 : 0;
            word = (0x10U << 26U) | (static_cast<std::uint32_t>(rs) << 21U) |
                   (static_cast<std::uint32_t>(rt) << 16U) | (24U << 11U) |
                   static_cast<std::uint32_t>(found->second);
            return true;
        }
    }
    if (mnemonic_ == "mfpc" || mnemonic_ == "mfps" ||
        mnemonic_ == "mtpc" || mnemonic_ == "mtps") {
        if (!need(2) || !reg(0, rt) || !imm(1, number)) goto fail;
        const bool counter = mnemonic_ == "mfpc" || mnemonic_ == "mtpc";
        const int max_register = counter ? 1 : 0;
        if (number < 0 || number > max_register) {
            error_ = "'" + mnemonic_ + "' register number must be " +
                     (counter ? std::string("0 or 1.") : std::string("0."));
            goto fail;
        }
        const bool move_to = mnemonic_ == "mtpc" || mnemonic_ == "mtps";
        rs = move_to ? 4 : 0;
        word = (0x10U << 26U) | (static_cast<std::uint32_t>(rs) << 21U) |
               (static_cast<std::uint32_t>(rt) << 16U) | (25U << 11U) |
               (static_cast<std::uint32_t>(number) << 1U) | (counter ? 1U : 0U);
        return true;
    }
    if (mnemonic_ == "mfc0" || mnemonic_ == "mtc0") {
        if (!need(2)) goto fail;
        if (!reg(0, rt) || !try_parse_cop_register(operands_[1], rd)) {
            if (error_.empty()) error_ = "invalid COP0 register '" + (operands_.size() > 1U ? operands_[1] : std::string{}) + "'.";
            goto fail;
        }
        rs = mnemonic_ == "mfc0" ? 0 : 4;
        word = (0x10U << 26U) | (static_cast<std::uint32_t>(rs) << 21U) |
               (static_cast<std::uint32_t>(rt) << 16U) | (static_cast<std::uint32_t>(rd) << 11U);
        return true;
    }
    if (mnemonic_ == "bc2f" || mnemonic_ == "bc2t" || mnemonic_ == "bc2fl" || mnemonic_ == "bc2tl") {
        if (!need(1) || !branch_offset(0, branch)) goto fail;
        rt = mnemonic_ == "bc2f" ? 0 :
             mnemonic_ == "bc2t" ? 1 :
             mnemonic_ == "bc2fl" ? 2 : 3;
        word = (0x12U << 26U) | (8U << 21U) | (static_cast<std::uint32_t>(rt) << 16U) | branch;
        return true;
    }
    if (mnemonic_ == "qmfc2" || mnemonic_ == "qmfc2.i" || mnemonic_ == "qmfc2.ni" ||
        mnemonic_ == "qmtc2" || mnemonic_ == "qmtc2.i" || mnemonic_ == "qmtc2.ni" ||
        mnemonic_ == "cfc2" || mnemonic_ == "cfc2.i" || mnemonic_ == "cfc2.ni" ||
        mnemonic_ == "ctc2" || mnemonic_ == "ctc2.i" || mnemonic_ == "ctc2.ni") {
        if (!need(2) || !reg(0, rt)) goto fail;
        const bool floating = mnemonic_.rfind("qm", 0) == 0;
        if (floating) {
            if (!c2reg(1, rd)) goto fail;
        } else if (!vireg(1, rd)) {
            goto fail;
        }
        if (mnemonic_.rfind("qmfc2", 0) == 0) rs = 1;
        else if (mnemonic_.rfind("cfc2", 0) == 0) rs = 2;
        else if (mnemonic_.rfind("qmtc2", 0) == 0) rs = 5;
        else rs = 6;
        const bool interlocked = mnemonic_.size() >= 2U && mnemonic_.compare(mnemonic_.size() - 2U, 2U, ".i") == 0;
        word = (0x12U << 26U) | (static_cast<std::uint32_t>(rs) << 21U) |
               (static_cast<std::uint32_t>(rt) << 16U) | (static_cast<std::uint32_t>(rd) << 11U) |
               (interlocked ? 1U : 0U);
        return true;
    }
    {
        std::string vu_base;
        int destination_mask = 0;
        std::string vu_error;
        if (parse_vu_mnemonic(mnemonic_, vu_base, destination_mask, vu_error)) {
            if (!vu_error.empty()) { error_ = vu_error; goto fail; }
            if (!need(3) || !c2reg(0, fd) || !c2reg(1, fs) || !c2reg(2, ft)) goto fail;
            const VuMacroInstruction* instruction = find_vu_macro_instruction(vu_base);
            if (instruction == nullptr) { error_ = "unsupported VU0 macro instruction '" + vu_base + "'."; goto fail; }
            word = (0x12U << 26U) |
                   (static_cast<std::uint32_t>(0x10 | destination_mask) << 21U) |
                   (static_cast<std::uint32_t>(ft) << 16U) |
                   (static_cast<std::uint32_t>(fs) << 11U) |
                   (static_cast<std::uint32_t>(fd) << 6U) |
                   static_cast<std::uint32_t>(instruction->funct);
            return true;
        }
    }
    if (mnemonic_ == "mfc1" || mnemonic_ == "cfc1" || mnemonic_ == "mtc1" || mnemonic_ == "ctc1") {
        if (!need(2) || !reg(0, rt) || !freg(1, fs)) goto fail;
        if (mnemonic_ == "cfc1" && fs != 0 && fs != 31) {
            error_ = "'cfc1' only supports FPU control registers $f0 and $f31 on the R5900.";
            goto fail;
        }
        if (mnemonic_ == "ctc1" && fs != 31) {
            error_ = "'ctc1' only supports FPU control register $f31 on the R5900.";
            goto fail;
        }
        rs = mnemonic_ == "mfc1" ? 0 : (mnemonic_ == "cfc1" ? 2 : (mnemonic_ == "mtc1" ? 4 : 6));
        word = (0x11U << 26U) | (static_cast<std::uint32_t>(rs) << 21U) |
               (static_cast<std::uint32_t>(rt) << 16U) | (static_cast<std::uint32_t>(fs) << 11U);
        return true;
    }
    if (mnemonic_ == "bc1f" || mnemonic_ == "bc1t" || mnemonic_ == "bc1fl" || mnemonic_ == "bc1tl") {
        if (!need(1) || !branch_offset(0, branch)) goto fail;
        rt = mnemonic_ == "bc1f" ? 0 :
             mnemonic_ == "bc1t" ? 1 :
             mnemonic_ == "bc1fl" ? 2 : 3;
        word = (0x11U << 26U) | (8U << 21U) | (static_cast<std::uint32_t>(rt) << 16U) | branch;
        return true;
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"add.s", 0}, {"sub.s", 1}, {"mul.s", 2}, {"div.s", 3}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !freg(0, fd) || !freg(1, fs) || !freg(2, ft)) goto fail;
            word = (0x11U << 26U) | (16U << 21U) | (static_cast<std::uint32_t>(ft) << 16U) |
                   (static_cast<std::uint32_t>(fs) << 11U) | (static_cast<std::uint32_t>(fd) << 6U) |
                   static_cast<std::uint32_t>(found->second);
            return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"rsqrt.s", 0x16}, {"madd.s", 0x1C}, {"msub.s", 0x1D},
            {"max.s", 0x28}, {"min.s", 0x29}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(3) || !freg(0, fd) || !freg(1, fs) || !freg(2, ft)) goto fail;
            word = (0x11U << 26U) | (16U << 21U) | (static_cast<std::uint32_t>(ft) << 16U) |
                   (static_cast<std::uint32_t>(fs) << 11U) | (static_cast<std::uint32_t>(fd) << 6U) |
                   static_cast<std::uint32_t>(found->second);
            return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"adda.s", 0x18}, {"suba.s", 0x19}, {"mula.s", 0x1A},
            {"madda.s", 0x1E}, {"msuba.s", 0x1F}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(2) || !freg(0, fs) || !freg(1, ft)) goto fail;
            word = (0x11U << 26U) | (16U << 21U) | (static_cast<std::uint32_t>(ft) << 16U) |
                   (static_cast<std::uint32_t>(fs) << 11U) |
                   static_cast<std::uint32_t>(found->second);
            return true;
        }
    }
    {
        static const std::unordered_map<std::string, int> operations{
            {"abs.s", 5}, {"mov.s", 6}, {"neg.s", 7}, {"cvt.w.s", 0x24}
        };
        const auto found = operations.find(mnemonic_);
        if (found != operations.end()) {
            if (!need(2) || !freg(0, fd) || !freg(1, fs)) goto fail;
            word = (0x11U << 26U) | (16U << 21U) | (static_cast<std::uint32_t>(fs) << 11U) |
                   (static_cast<std::uint32_t>(fd) << 6U) | static_cast<std::uint32_t>(found->second);
            return true;
        }
    }
    if (mnemonic_ == "sqrt.s") {
        if (!need(2) || !freg(0, fd) || !freg(1, ft)) goto fail;
        word = (0x11U << 26U) | (16U << 21U) | (static_cast<std::uint32_t>(ft) << 16U) |
               (static_cast<std::uint32_t>(fd) << 6U) | 0x04U;
        return true;
    }
    if (mnemonic_ == "cvt.s.w") {
        if (!need(2) || !freg(0, fd) || !freg(1, fs)) goto fail;
        word = (0x11U << 26U) | (20U << 21U) | (static_cast<std::uint32_t>(fs) << 11U) |
               (static_cast<std::uint32_t>(fd) << 6U) | 0x20U;
        return true;
    }
    if (mnemonic_ == "c.f.s" || mnemonic_ == "c.eq.s" || mnemonic_ == "c.lt.s" || mnemonic_ == "c.le.s") {
        if (!need(2) || !freg(0, fs) || !freg(1, ft)) goto fail;
        const int funct = mnemonic_ == "c.f.s" ? 0x30 :
                          (mnemonic_ == "c.eq.s" ? 0x32 : (mnemonic_ == "c.lt.s" ? 0x34 : 0x36));
        word = (0x11U << 26U) | (16U << 21U) | (static_cast<std::uint32_t>(ft) << 16U) |
               (static_cast<std::uint32_t>(fs) << 11U) | static_cast<std::uint32_t>(funct);
        return true;
    }

    error_ = "unsupported R5900 mnemonic '" + mnemonic_ + "'. Use .word 0xXXXXXXXX to preserve an opcode exactly.";

fail:
    error = error_;
    return false;
}

bool try_assemble_instruction(std::string_view text, std::uint32_t pc,
                              const std::unordered_map<std::string, std::uint32_t>& labels,
                              std::uint32_t& word, std::string& error) {
    Assembler assembler(text, pc, labels);
    return assembler.run(word, error);
}

bool try_get_control_flow_target(std::uint32_t word, std::uint32_t pc, std::uint32_t& target) {
    target = 0U;
    const std::uint32_t op = word >> 26U;
    if (op == 2U || op == 3U) {
        target = ((pc + 4U) & 0xF0000000U) | ((word & 0x03FFFFFFU) << 2U);
        return true;
    }
    const std::uint32_t rt = (word >> 16U) & 0x1FU;
    const bool regimm_branch = op == 1U &&
        (rt == 0x00U || rt == 0x01U || rt == 0x02U || rt == 0x03U ||
         rt == 0x10U || rt == 0x11U || rt == 0x12U || rt == 0x13U);
    const bool two_register_branch = op == 4U || op == 5U || op == 0x14U || op == 0x15U;
    const bool zero_compare_branch =
        (op == 6U || op == 7U || op == 0x16U || op == 0x17U) && rt == 0U;
    if (regimm_branch || two_register_branch || zero_compare_branch) {
        const auto imm = static_cast<std::int16_t>(word & 0xFFFFU);
        target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) +
                                            (static_cast<std::int64_t>(imm) * 4));
        return true;
    }
    if ((op == 0x10U || op == 0x11U || op == 0x12U) &&
        ((word >> 21U) & 0x1FU) == 8U && rt <= 3U) {
        const auto imm = static_cast<std::int16_t>(word & 0xFFFFU);
        target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) +
                                            (static_cast<std::int64_t>(imm) * 4));
        return true;
    }
    return false;
}

std::unordered_map<std::uint32_t, std::string> find_control_flow_targets(
    const Cheat& cheat, const std::unordered_set<std::uint32_t>& direct_addresses) {
    std::unordered_map<std::uint32_t, std::string> result;
    for (std::size_t index = 0; index + 1U < cheat.words.size(); index += 2U) {
        const std::uint32_t raw_address = cheat.words[index];
        if ((raw_address >> 28U) != 2U) continue;
        const std::uint32_t pc = raw_address & 0x0FFFFFFFU;
        std::uint32_t target = 0U;
        if (!try_get_control_flow_target(cheat.words[index + 1U], pc, target) ||
            direct_addresses.find(target) == direct_addresses.end()) continue;
        result.emplace(target, "loc_" + format_hex(target, 8));
    }
    return result;
}

std::string disassemble_word(std::uint32_t word, std::uint32_t pc,
                             const std::unordered_map<std::uint32_t, std::string>& labels) {
    if (word == 0U) return "nop";

    const std::uint32_t op = word >> 26U;
    const int rs = static_cast<int>((word >> 21U) & 31U);
    const int rt = static_cast<int>((word >> 16U) & 31U);
    const int rd = static_cast<int>((word >> 11U) & 31U);
    const int shamt = static_cast<int>((word >> 6U) & 31U);
    const int funct = static_cast<int>(word & 63U);
    const auto imm = static_cast<std::uint16_t>(word & 0xFFFFU);
    const auto simm = static_cast<std::int16_t>(imm);

    const auto rg = [](int value) { return std::string(register_names[static_cast<std::size_t>(value)]); };
    const auto fg = [](int value) { return "$f" + std::to_string(value); };
    const auto target_name = [&](std::uint32_t address) {
        const auto found = labels.find(address);
        return found != labels.end() ? found->second : "0x" + format_hex(address, 8);
    };
    const auto mem = [&] { return std::to_string(simm) + "(" + rg(rs) + ")"; };
    const auto raw = [&] { return ".word 0x" + format_hex(word, 8); };

    if (op == 0U) {
        switch (funct) {
            case 0x00: if (rs == 0) return "sll " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x02: if (rs == 0) return "srl " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x03: if (rs == 0) return "sra " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x04: if (shamt == 0) return "sllv " + rg(rd) + ", " + rg(rt) + ", " + rg(rs); break;
            case 0x06: if (shamt == 0) return "srlv " + rg(rd) + ", " + rg(rt) + ", " + rg(rs); break;
            case 0x07: if (shamt == 0) return "srav " + rg(rd) + ", " + rg(rt) + ", " + rg(rs); break;
            case 0x14: if (shamt == 0) return "dsllv " + rg(rd) + ", " + rg(rt) + ", " + rg(rs); break;
            case 0x16: if (shamt == 0) return "dsrlv " + rg(rd) + ", " + rg(rt) + ", " + rg(rs); break;
            case 0x17: if (shamt == 0) return "dsrav " + rg(rd) + ", " + rg(rt) + ", " + rg(rs); break;
            case 0x08: if (rt == 0 && rd == 0 && shamt == 0) return "jr " + rg(rs); break;
            case 0x09:
                if (rt == 0 && shamt == 0) return rd == 31 ? "jalr " + rg(rs) : "jalr " + rg(rd) + ", " + rg(rs);
                break;
            case 0x0A: if (shamt == 0) return "movz " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x0B: if (shamt == 0) return "movn " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x0C: {
                const std::uint32_t code = (word >> 6U) & 0xFFFFFU;
                return code == 0U ? "syscall" : "syscall 0x" + format_hex(code, 0);
            }
            case 0x0D: {
                const std::uint32_t code = (word >> 6U) & 0xFFFFFU;
                return code == 0U ? "break" : "break 0x" + format_hex(code, 0);
            }
            case 0x0F: if (rs == 0 && rt == 0 && rd == 0) return shamt == 0 ? "sync" : "sync " + std::to_string(shamt); break;
            case 0x10: if (rs == 0 && rt == 0 && shamt == 0) return "mfhi " + rg(rd); break;
            case 0x11: if (rt == 0 && rd == 0 && shamt == 0) return "mthi " + rg(rs); break;
            case 0x12: if (rs == 0 && rt == 0 && shamt == 0) return "mflo " + rg(rd); break;
            case 0x13: if (rt == 0 && rd == 0 && shamt == 0) return "mtlo " + rg(rs); break;
            case 0x18:
                if (shamt == 0) return rd == 0 ? "mult " + rg(rs) + ", " + rg(rt) :
                    "mult " + rg(rd) + ", " + rg(rs) + ", " + rg(rt);
                break;
            case 0x19:
                if (shamt == 0) return rd == 0 ? "multu " + rg(rs) + ", " + rg(rt) :
                    "multu " + rg(rd) + ", " + rg(rs) + ", " + rg(rt);
                break;
            case 0x1A: if (rd == 0 && shamt == 0) return "div " + rg(rs) + ", " + rg(rt); break;
            case 0x1B: if (rd == 0 && shamt == 0) return "divu " + rg(rs) + ", " + rg(rt); break;
            case 0x20: if (shamt == 0) return "add " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x21: if (shamt == 0) return "addu " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x22: if (shamt == 0) return "sub " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x23: if (shamt == 0) return "subu " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x24: if (shamt == 0) return "and " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x25: if (shamt == 0) return "or " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x26: if (shamt == 0) return "xor " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x27: if (shamt == 0) return "nor " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x2A: if (shamt == 0) return "slt " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x2B: if (shamt == 0) return "sltu " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x2C: if (shamt == 0) return "dadd " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x2D: if (shamt == 0) return "daddu " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x2E: if (shamt == 0) return "dsub " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x28: if (rs == 0 && rt == 0 && shamt == 0) return "mfsa " + rg(rd); break;
            case 0x29: if (rt == 0 && rd == 0 && shamt == 0) return "mtsa " + rg(rs); break;
            case 0x2F: if (shamt == 0) return "dsubu " + rg(rd) + ", " + rg(rs) + ", " + rg(rt); break;
            case 0x30:
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x36: {
                const char* mnemonic = funct == 0x30 ? "tge" :
                                       funct == 0x31 ? "tgeu" :
                                       funct == 0x32 ? "tlt" :
                                       funct == 0x33 ? "tltu" :
                                       funct == 0x34 ? "teq" : "tne";
                const std::uint32_t code = (word >> 6U) & 0x3FFU;
                return std::string(mnemonic) + " " + rg(rs) + ", " + rg(rt) +
                       (code == 0U ? "" : ", 0x" + format_hex(code, 0));
            }
            case 0x38: if (rs == 0) return "dsll " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x3A: if (rs == 0) return "dsrl " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x3B: if (rs == 0) return "dsra " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x3C: if (rs == 0) return "dsll32 " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x3E: if (rs == 0) return "dsrl32 " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            case 0x3F: if (rs == 0) return "dsra32 " + rg(rd) + ", " + rg(rt) + ", " + std::to_string(shamt); break;
            default: break;
        }
        return raw();
    }

    if (op == 0x1CU) {
        const auto two_or_three = [&](std::string_view mnemonic) {
            const std::string prefix(mnemonic);
            return rd == 0 ? prefix + " " + rg(rs) + ", " + rg(rt) :
                             prefix + " " + rg(rd) + ", " + rg(rs) + ", " + rg(rt);
        };

        if (rs == 0) {
            const char* packed_shift = funct == 0x34 ? "psllh" :
                                       funct == 0x36 ? "psrlh" :
                                       funct == 0x37 ? "psrah" :
                                       funct == 0x3C ? "psllw" :
                                       funct == 0x3E ? "psrlw" :
                                       funct == 0x3F ? "psraw" : nullptr;
            if (packed_shift != nullptr) {
                const bool halfword_shift = funct == 0x34 || funct == 0x36 || funct == 0x37;
                if (!halfword_shift || shamt <= 15) {
                    return std::string(packed_shift) + " " + rg(rd) + ", " + rg(rt) +
                           ", " + std::to_string(shamt);
                }
                return raw();
            }
        }

        if (funct == 0x30 && rs == 0 && rt == 0 && shamt >= 0 && shamt <= 4) {
            const char* mnemonic = shamt == 0 ? "pmfhl.lw" :
                                   shamt == 1 ? "pmfhl.uw" :
                                   shamt == 2 ? "pmfhl.slw" :
                                   shamt == 3 ? "pmfhl.lh" : "pmfhl.sh";
            return std::string(mnemonic) + " " + rg(rd);
        }
        if (funct == 0x31 && rt == 0 && rd == 0 && shamt == 0) {
            return "pmthl.lw " + rg(rs);
        }

        if (const MmiInstruction* instruction = decode_mmi_instruction(funct, shamt)) {
            const std::string mnemonic(instruction->mnemonic);
            switch (instruction->operands) {
                case MmiOperandForm::d_s_t:
                    return mnemonic + " " + rg(rd) + ", " + rg(rs) + ", " + rg(rt);
                case MmiOperandForm::d_t_s:
                    return mnemonic + " " + rg(rd) + ", " + rg(rt) + ", " + rg(rs);
                case MmiOperandForm::d_t:
                    if (rs == 0) return mnemonic + " " + rg(rd) + ", " + rg(rt);
                    break;
                case MmiOperandForm::s_t:
                    if (rd == 0) return mnemonic + " " + rg(rs) + ", " + rg(rt);
                    break;
                case MmiOperandForm::d:
                    if (rs == 0 && rt == 0) return mnemonic + " " + rg(rd);
                    break;
                case MmiOperandForm::s:
                    if (rt == 0 && rd == 0) return mnemonic + " " + rg(rs);
                    break;
            }
            return raw();
        }

        if (shamt == 0) {
            switch (funct) {
                case 0x00: return two_or_three("madd");
                case 0x01: return two_or_three("maddu");
                case 0x04: if (rt == 0) return "plzcw " + rg(rd) + ", " + rg(rs); break;
                case 0x10: if (rs == 0 && rt == 0) return "mfhi1 " + rg(rd); break;
                case 0x11: if (rt == 0 && rd == 0) return "mthi1 " + rg(rs); break;
                case 0x12: if (rs == 0 && rt == 0) return "mflo1 " + rg(rd); break;
                case 0x13: if (rt == 0 && rd == 0) return "mtlo1 " + rg(rs); break;
                case 0x18: return two_or_three("mult1");
                case 0x19: return two_or_three("multu1");
                case 0x1A: if (rd == 0) return "div1 " + rg(rs) + ", " + rg(rt); break;
                case 0x1B: if (rd == 0) return "divu1 " + rg(rs) + ", " + rg(rt); break;
                case 0x20: return two_or_three("madd1");
                case 0x21: return two_or_three("maddu1");
                default: break;
            }
        }
        return raw();
    }

    switch (op) {
        case 0x01: {
            if (rt == 0x18) return "mtsab " + rg(rs) + ", " + std::to_string(simm);
            if (rt == 0x19) return "mtsah " + rg(rs) + ", " + std::to_string(simm);
            const char* trap = rt == 0x08 ? "tgei" :
                               rt == 0x09 ? "tgeiu" :
                               rt == 0x0A ? "tlti" :
                               rt == 0x0B ? "tltiu" :
                               rt == 0x0C ? "teqi" :
                               rt == 0x0E ? "tnei" : nullptr;
            if (trap != nullptr) return std::string(trap) + " " + rg(rs) + ", " + std::to_string(simm);
            const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + (static_cast<std::int64_t>(simm) * 4));
            std::string mnemonic;
            if (rt == 0x00) mnemonic = "bltz";
            else if (rt == 0x01) mnemonic = "bgez";
            else if (rt == 0x02) mnemonic = "bltzl";
            else if (rt == 0x03) mnemonic = "bgezl";
            else if (rt == 0x10) mnemonic = "bltzal";
            else if (rt == 0x11) mnemonic = "bgezal";
            else if (rt == 0x12) mnemonic = "bltzall";
            else if (rt == 0x13) mnemonic = "bgezall";
            return mnemonic.empty() ? raw() : mnemonic + " " + rg(rs) + ", " + target_name(target);
        }
        case 0x02:
        case 0x03: {
            const std::uint32_t target = ((pc + 4U) & 0xF0000000U) | ((word & 0x03FFFFFFU) << 2U);
            return std::string(op == 2U ? "j " : "jal ") + target_name(target);
        }
        case 0x04:
        case 0x05:
        case 0x14:
        case 0x15: {
            const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + (static_cast<std::int64_t>(simm) * 4));
            const char* mnemonic = op == 0x04U ? "beq " :
                                   op == 0x05U ? "bne " :
                                   op == 0x14U ? "beql " : "bnel ";
            return std::string(mnemonic) + rg(rs) + ", " + rg(rt) + ", " + target_name(target);
        }
        case 0x06:
        case 0x07:
        case 0x16:
        case 0x17:
            if (rt == 0) {
                const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + (static_cast<std::int64_t>(simm) * 4));
                const char* mnemonic = op == 0x06U ? "blez " :
                                       op == 0x07U ? "bgtz " :
                                       op == 0x16U ? "blezl " : "bgtzl ";
                return std::string(mnemonic) + rg(rs) + ", " + target_name(target);
            }
            break;
        case 0x08: return "addi " + rg(rt) + ", " + rg(rs) + ", " + std::to_string(simm);
        case 0x09: return "addiu " + rg(rt) + ", " + rg(rs) + ", " + std::to_string(simm);
        case 0x0A: return "slti " + rg(rt) + ", " + rg(rs) + ", " + std::to_string(simm);
        case 0x0B: return "sltiu " + rg(rt) + ", " + rg(rs) + ", " + std::to_string(simm);
        case 0x0C: return "andi " + rg(rt) + ", " + rg(rs) + ", 0x" + format_hex(imm, 4);
        case 0x0D: return "ori " + rg(rt) + ", " + rg(rs) + ", 0x" + format_hex(imm, 4);
        case 0x0E: return "xori " + rg(rt) + ", " + rg(rs) + ", 0x" + format_hex(imm, 4);
        case 0x0F: if (rs == 0) return "lui " + rg(rt) + ", 0x" + format_hex(imm, 4); break;
        case 0x18: return "daddi " + rg(rt) + ", " + rg(rs) + ", " + std::to_string(simm);
        case 0x19: return "daddiu " + rg(rt) + ", " + rg(rs) + ", " + std::to_string(simm);
        case 0x1A: return "ldl " + rg(rt) + ", " + mem();
        case 0x1B: return "ldr " + rg(rt) + ", " + mem();
        case 0x1E: return "lq " + rg(rt) + ", " + mem();
        case 0x1F: return "sq " + rg(rt) + ", " + mem();
        case 0x20: return "lb " + rg(rt) + ", " + mem();
        case 0x21: return "lh " + rg(rt) + ", " + mem();
        case 0x22: return "lwl " + rg(rt) + ", " + mem();
        case 0x23: return "lw " + rg(rt) + ", " + mem();
        case 0x24: return "lbu " + rg(rt) + ", " + mem();
        case 0x25: return "lhu " + rg(rt) + ", " + mem();
        case 0x26: return "lwr " + rg(rt) + ", " + mem();
        case 0x27: return "lwu " + rg(rt) + ", " + mem();
        case 0x28: return "sb " + rg(rt) + ", " + mem();
        case 0x29: return "sh " + rg(rt) + ", " + mem();
        case 0x2A: return "swl " + rg(rt) + ", " + mem();
        case 0x2B: return "sw " + rg(rt) + ", " + mem();
        case 0x2C: return "sdl " + rg(rt) + ", " + mem();
        case 0x2D: return "sdr " + rg(rt) + ", " + mem();
        case 0x2E: return "swr " + rg(rt) + ", " + mem();
        case 0x2F: return "cache " + std::to_string(rt) + ", " + mem();
        case 0x31: return "lwc1 " + fg(rt) + ", " + mem();
        case 0x33: return "pref " + std::to_string(rt) + ", " + mem();
        case 0x36: return "lqc2 $vf" + std::to_string(rt) + ", " + mem();
        case 0x37: return "ld " + rg(rt) + ", " + mem();
        case 0x39: return "swc1 " + fg(rt) + ", " + mem();
        case 0x3E: return "sqc2 $vf" + std::to_string(rt) + ", " + mem();
        case 0x3F: return "sd " + rg(rt) + ", " + mem();
        default: break;
    }

    if (op == 0x10U) {
        if (rs == 8 && rt >= 0 && rt <= 3) {
            const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + (static_cast<std::int64_t>(simm) * 4));
            const char* mnemonic = rt == 0 ? "bc0f " :
                                   rt == 1 ? "bc0t " :
                                   rt == 2 ? "bc0fl " : "bc0tl ";
            return std::string(mnemonic) + target_name(target);
        }
        if (word == 0x42000001U) return "tlbr";
        if (word == 0x42000002U) return "tlbwi";
        if (word == 0x42000006U) return "tlbwr";
        if (word == 0x42000008U) return "tlbp";
        if (word == 0x42000018U) return "eret";
        if (word == 0x42000038U) return "ei";
        if (word == 0x42000039U) return "di";
        if (rs == 0 || rs == 4) {
            const bool move_to = rs == 4;
            const std::uint32_t low11 = word & 0x7FFU;
            if (rd == 24) {
                const char* mnemonic = nullptr;
                switch (low11) {
                    case 0: mnemonic = move_to ? "mtbpc " : "mfbpc "; break;
                    case 2: mnemonic = move_to ? "mtiab " : "mfiab "; break;
                    case 3: mnemonic = move_to ? "mtiabm " : "mfiabm "; break;
                    case 4: mnemonic = move_to ? "mtdab " : "mfdab "; break;
                    case 5: mnemonic = move_to ? "mtdabm " : "mfdabm "; break;
                    case 6: mnemonic = move_to ? "mtdvb " : "mfdvb "; break;
                    case 7: mnemonic = move_to ? "mtdvbm " : "mfdvbm "; break;
                    default: break;
                }
                if (mnemonic != nullptr) return std::string(mnemonic) + rg(rt);
                return raw();
            }
            if (rd == 25 && (low11 & 0x7C0U) == 0U) {
                const int performance_register = static_cast<int>((low11 >> 1U) & 31U);
                const bool counter = (low11 & 1U) != 0U;
                if ((counter && performance_register <= 1) ||
                    (!counter && performance_register == 0)) {
                    const char* mnemonic = move_to
                        ? (counter ? "mtpc " : "mtps ")
                        : (counter ? "mfpc " : "mfps ");
                    return std::string(mnemonic) + rg(rt) + ", " +
                           std::to_string(performance_register);
                }
                return raw();
            }
            if (low11 == 0U) {
                return std::string(move_to ? "mtc0 " : "mfc0 ") + rg(rt) +
                       ", $c" + std::to_string(rd);
            }
        }
        return raw();
    }

    if (op == 0x12U) {
        if (rs == 8 && rt >= 0 && rt <= 3) {
            const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + (static_cast<std::int64_t>(simm) * 4));
            const char* mnemonic = rt == 0 ? "bc2f " :
                                   rt == 1 ? "bc2t " :
                                   rt == 2 ? "bc2fl " : "bc2tl ";
            return std::string(mnemonic) + target_name(target);
        }
        if (rs == 1 || rs == 2 || rs == 5 || rs == 6) {
            if ((word & 0x7FEU) != 0U) return raw();
            const bool interlocked = (word & 1U) != 0U;
            const std::string suffix = interlocked ? ".i" : "";
            if (rs == 1) return std::string("qmfc2") + suffix + " " + rg(rt) + ", $vf" + std::to_string(rd);
            if (rs == 2) return std::string("cfc2") + suffix + " " + rg(rt) + ", $vi" + std::to_string(rd);
            if (rs == 5) return std::string("qmtc2") + suffix + " " + rg(rt) + ", $vf" + std::to_string(rd);
            return std::string("ctc2") + suffix + " " + rg(rt) + ", $vi" + std::to_string(rd);
        }
        if (rs >= 16) {
            const int destination_mask = rs & 0xF;
            if (destination_mask == 0) return raw();
            if (const VuMacroInstruction* instruction = decode_vu_macro_instruction(funct)) {
                return std::string(instruction->mnemonic) + "." +
                       format_vu_destination_mask(destination_mask) + " $vf" +
                       std::to_string(shamt) + ", $vf" + std::to_string(rd) +
                       ", $vf" + std::to_string(rt);
            }
        }
        return raw();
    }

    if (op == 0x11U) {
        const int fmt = rs;
        const int f_rt = rt;
        const int fs = rd;
        const int fd = shamt;
        if (fmt == 0 && (word & 0x7FFU) == 0U) return "mfc1 " + rg(rt) + ", " + fg(fs);
        if (fmt == 2 && (word & 0x7FFU) == 0U && (fs == 0 || fs == 31)) return "cfc1 " + rg(rt) + ", " + fg(fs);
        if (fmt == 4 && (word & 0x7FFU) == 0U) return "mtc1 " + rg(rt) + ", " + fg(fs);
        if (fmt == 6 && (word & 0x7FFU) == 0U && fs == 31) return "ctc1 " + rg(rt) + ", " + fg(fs);
        if (fmt == 8 && rt >= 0 && rt <= 3) {
            const std::uint32_t target = static_cast<std::uint32_t>(static_cast<std::int64_t>(pc + 4U) + (static_cast<std::int64_t>(simm) * 4));
            const char* mnemonic = rt == 0 ? "bc1f " :
                                   rt == 1 ? "bc1t " :
                                   rt == 2 ? "bc1fl " : "bc1tl ";
            return std::string(mnemonic) + target_name(target);
        }
        if (fmt == 16) {
            switch (funct) {
                case 0x00: return "add.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x01: return "sub.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x02: return "mul.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x03: return "div.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x04: if (fs == 0) return "sqrt.s " + fg(fd) + ", " + fg(f_rt); break;
                case 0x05: if (f_rt == 0) return "abs.s " + fg(fd) + ", " + fg(fs); break;
                case 0x06: if (f_rt == 0) return "mov.s " + fg(fd) + ", " + fg(fs); break;
                case 0x07: if (f_rt == 0) return "neg.s " + fg(fd) + ", " + fg(fs); break;
                case 0x16: return "rsqrt.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x18: if (fd == 0) return "adda.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x19: if (fd == 0) return "suba.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x1A: if (fd == 0) return "mula.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x1C: return "madd.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x1D: return "msub.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x1E: if (fd == 0) return "madda.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x1F: if (fd == 0) return "msuba.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x24: if (f_rt == 0) return "cvt.w.s " + fg(fd) + ", " + fg(fs); break;
                case 0x28: return "max.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x29: return "min.s " + fg(fd) + ", " + fg(fs) + ", " + fg(f_rt);
                case 0x30: if (fd == 0) return "c.f.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x32: if (fd == 0) return "c.eq.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x34: if (fd == 0) return "c.lt.s " + fg(fs) + ", " + fg(f_rt); break;
                case 0x36: if (fd == 0) return "c.le.s " + fg(fs) + ", " + fg(f_rt); break;
                default: break;
            }
            return raw();
        }
        if (fmt == 20 && funct == 0x20 && f_rt == 0) return "cvt.s.w " + fg(fd) + ", " + fg(fs);
    }
    return raw();
}

} // namespace

bool ParseResult::success() const noexcept {
    return errors.empty() && std::any_of(blocks.begin(), blocks.end(), [](const text::CheatBlock& block) {
        return !block.cheat.empty();
    });
}

ParseResult parse_text(std::string_view input) {
    ParseResult result;
    if (hex::trim(input).empty()) {
        result.errors.emplace_back("No PS2 MIPS source was provided.");
        return result;
    }

    const std::vector<SourceBlock> sources = parse_source_blocks(input, result.errors);
    if (!result.errors.empty()) return result;

    for (const SourceBlock& source : sources) {
        text::CheatBlock block;
        block.label = source.name;
        if (source.name) block.cheat.name = *source.name;
        for (const SourceItem& item : source.items) {
            switch (item.kind) {
                case ItemKind::raw_code:
                    block.cheat.append_pair(item.raw_address, item.raw_value);
                    break;
                case ItemKind::word: {
                    std::int64_t value = 0;
                    if (!try_parse_integer(item.text, source.labels, value)) {
                        result.errors.push_back("Line " + std::to_string(item.line_number) + ": invalid .word value '" + item.text + "'.");
                    } else {
                        append_direct_write(block.cheat, 2, item.address, static_cast<std::uint32_t>(value));
                    }
                    break;
                }
                case ItemKind::half: {
                    std::int64_t value = 0;
                    if (!try_parse_integer(item.text, source.labels, value)) {
                        result.errors.push_back("Line " + std::to_string(item.line_number) + ": invalid .half value '" + item.text + "'.");
                    } else {
                        append_direct_write(block.cheat, 1, item.address, static_cast<std::uint32_t>(value) & 0xFFFFU);
                    }
                    break;
                }
                case ItemKind::byte: {
                    std::int64_t value = 0;
                    if (!try_parse_integer(item.text, source.labels, value)) {
                        result.errors.push_back("Line " + std::to_string(item.line_number) + ": invalid .byte value '" + item.text + "'.");
                    } else {
                        append_direct_write(block.cheat, 0, item.address, static_cast<std::uint32_t>(value) & 0xFFU);
                    }
                    break;
                }
                case ItemKind::instruction: {
                    std::uint32_t word = 0U;
                    std::string error;
                    if (!try_assemble_instruction(item.text, item.address, source.labels, word, error)) {
                        result.errors.push_back("Line " + std::to_string(item.line_number) + ": " + error);
                    } else {
                        append_direct_write(block.cheat, 2, item.address, word);
                    }
                    break;
                }
            }
        }
        result.blocks.push_back(std::move(block));
    }

    if (!std::any_of(result.blocks.begin(), result.blocks.end(), [](const text::CheatBlock& block) {
            return !block.cheat.empty();
        }) && result.errors.empty()) {
        result.errors.emplace_back("No valid PS2 MIPS instructions or data directives were found.");
    }
    return result;
}

std::string write_text(const std::vector<text::CheatBlock>& blocks) {
    std::ostringstream out;
    out << "// PS2 MIPS R5900 / Emotion Engine\n"
        << "// 32-bit instructions, little-endian\n"
        << "// Unknown/noncanonical opcodes are preserved as .word values.\n"
        << "// Non-direct cheat types are preserved as .code ADDR VALUE.\n\n";

    for (std::size_t block_index = 0; block_index < blocks.size(); ++block_index) {
        const text::CheatBlock& block = blocks[block_index];
        std::string name = block.label.value_or(block.cheat.name);
        if (!name.empty()) out << name << '\n';
        if (block.conversion_error) {
            out << "// Conversion error: " << *block.conversion_error << "\n";
            if (block_index + 1U < blocks.size()) out << '\n';
            continue;
        }
        if (block.cheat.empty()) {
            if (block_index + 1U < blocks.size()) out << '\n';
            continue;
        }

        std::unordered_set<std::uint32_t> direct_addresses;
        for (std::size_t index = 0; index + 1U < block.cheat.words.size(); index += 2U) {
            const std::uint32_t raw_address = block.cheat.words[index];
            if ((raw_address >> 28U) == 2U) direct_addresses.insert(raw_address & 0x0FFFFFFFU);
        }
        const auto generated_labels = find_control_flow_targets(block.cheat, direct_addresses);
        std::optional<std::uint32_t> expected_address;

        for (std::size_t index = 0; index + 1U < block.cheat.words.size(); index += 2U) {
            const std::uint32_t raw_address = block.cheat.words[index];
            const std::uint32_t value = block.cheat.words[index + 1U];
            const std::uint32_t code_type = raw_address >> 28U;
            const std::uint32_t address = raw_address & 0x0FFFFFFFU;
            if (code_type <= 2U) {
                const std::uint32_t width = code_type == 0U ? 1U : (code_type == 1U ? 2U : 4U);
                if (!expected_address || *expected_address != address) out << ".org 0x" << format_hex(address, 8) << '\n';
                const auto label = generated_labels.find(address);
                if (label != generated_labels.end()) out << label->second << ":\n";
                if (code_type == 2U) {
                    const std::string assembly = disassemble_word(value, address, generated_labels);
                    out << std::left << std::setw(42) << assembly << std::right << " // " << format_hex(value, 8) << '\n';
                } else if (code_type == 1U) {
                    out << ".half 0x" << format_hex(value & 0xFFFFU, 4) << '\n';
                } else {
                    out << ".byte 0x" << format_hex(value & 0xFFU, 2) << '\n';
                }
                expected_address = address + width;
            } else {
                expected_address.reset();
                out << ".code 0x" << format_hex(raw_address, 8) << " 0x" << format_hex(value, 8) << '\n';
            }
        }
        if (block_index + 1U < blocks.size()) out << '\n';
    }

    std::string result = out.str();
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    result.push_back('\n');
    return result;
}

} // namespace omni::formats::mips_r5900
