#include "analysis/e001_fixer.hpp"
#include "util/hex.hpp"
#include "util/newlines.hpp"

#include <sstream>
#include <string>

namespace omni::analysis {
std::string rewrite_e001_to_d(std::string_view text) {
    const std::string normalized = newlines::to_lf(text);
    std::istringstream input(normalized);
    std::string output;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string a;
        std::string v;
        std::string extra;
        if ((row >> a >> v) && !(row >> extra) && a.size() == 8U && v.size() == 8U) {
            const auto address = hex::parse_u32(a);
            const auto value = hex::parse_u32(v);
            if (address && value && (*address & 0xFFFF0000U) == 0xE0010000U) {
                output += hex::format_u32(0xD0000000U | (*value & 0x0FFFFFFFU));
                output += ' ';
                output += hex::format_u32(*address & 0xFFFFU);
                output += '\n';
                continue;
            }
        }
        output += line;
        output += '\n';
    }
    return output;
}
} // namespace omni::analysis
