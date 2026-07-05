#include "tests/test_mips_r5900.hpp"

#include "convert/conversion_service.hpp"
#include "formats/mips/mips_r5900_text.hpp"
#include "tests/test_support.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace omni::tests {
namespace {

void test_format_identity() {
    require(parse_code_format("MIPS") == CodeFormat::ps2_mips_r5900,
            "MIPS format token was not recognized");
    require(parse_code_format("PS2 MIPS R5900") == CodeFormat::ps2_mips_r5900,
            "PS2 MIPS R5900 alias was not recognized");
    require(crypt_for_format(CodeFormat::ps2_mips_r5900) == Crypt::raw,
            "MIPS format should use RAW crypto");
    require(device_for_format(CodeFormat::ps2_mips_r5900) == Device::standard,
            "MIPS format should use Standard semantics");
}

void test_basic_code_cave() {
    constexpr std::string_view source =
        "Infinite Health\n"
        ".org 0x000A0000\n"
        "lui $at, 0x447A\n"
        "sw $at, 0x0B38($v0)\n"
        "jr $ra\n"
        "nop\n";
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "Basic R5900 code cave did not parse");
    require(parsed.blocks.size() == 1U, "Basic R5900 block count is incorrect");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == 8U, "Basic R5900 code cave word count is incorrect");
    require(words[0] == 0x200A0000U && words[1] == 0x3C01447AU,
            "lui encoding is incorrect");
    require(words[2] == 0x200A0004U && words[3] == 0xAC410B38U,
            "sw encoding is incorrect");
    require(words[4] == 0x200A0008U && words[5] == 0x03E00008U,
            "jr encoding is incorrect");
    require(words[6] == 0x200A000CU && words[7] == 0U,
            "nop encoding is incorrect");
}

void test_hook_shorthand() {
    constexpr std::string_view source =
        "0x001355B4 : Hook\n"
        "j 0x000A0180\n"
        "nop\n";
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "Numeric Hook shorthand did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == 4U && words[0] == 0x201355B4U && words[1] == 0x08028060U,
            "Hook jump was not emitted at the declared address");
    require(words[2] == 0x200A0180U && words[3] == 0U,
            "Hook shorthand did not continue at the numeric jump target");
}

void test_data_widths_and_raw_code() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        ".word 0x70000002\n"
        ".half 0x1234\n"
        ".byte 0x56\n"
        ".code 0xE0010001 0x00100000\n";
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "R5900 data directives did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == 8U, "R5900 directive word count is incorrect");
    require(words[0] == 0x20100000U && words[1] == 0x70000002U,
            ".word encoding is incorrect");
    require(words[2] == 0x10100004U && words[3] == 0x1234U,
            ".half encoding is incorrect");
    require(words[4] == 0x00100006U && words[5] == 0x56U,
            ".byte encoding is incorrect");
    require(words[6] == 0xE0010001U && words[7] == 0x00100000U,
            ".code did not preserve the raw row");

    const std::string output = formats::mips_r5900::write_text(parsed.blocks);
    require(output.find(".word 0x70000002") != std::string::npos,
            "Unknown/noncanonical word was not preserved as .word");
    require(output.find(".half 0x1234") != std::string::npos,
            "Halfword output is missing");
    require(output.find(".byte 0x56") != std::string::npos,
            "Byte output is missing");
    require(output.find(".code 0xE0010001 0x00100000") != std::string::npos,
            "Non-direct cheat row was not preserved as .code");
}

void test_branch_labels_and_families() {
    constexpr std::string_view source =
        ".org 0x000A0000\n"
        "beq $s0, $zero, done\n"
        "nop\n"
        "done: addiu $v0, $v0, 15912\n"
        "lq $t0, 16($sp)\n"
        "sq $t0, -16($sp)\n"
        "mfc0 $v0, $c12\n"
        "add.s $f0, $f1, $f2\n"
        "jr $ra\n"
        "nop\n";
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "Representative R5900 families did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words[1] == 0x12000001U, "Branch label offset is incorrect");
    require(words[5] == 0x24423E28U, "Decimal addiu immediate is incorrect");
    require(words[7] == 0x7BA80010U, "lq encoding is incorrect");
    require(words[9] == 0x7FA8FFF0U, "sq encoding is incorrect");
    require(words[11] == 0x40026000U, "mfc0 encoding is incorrect");
    require(words[13] == 0x46020800U, "add.s encoding is incorrect");

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("loc_000A0008:") != std::string::npos,
            "Formatter did not generate a local branch label");
    require(formatted.find("beq $s0, $zero, loc_000A0008") != std::string::npos,
            "Formatter did not use the generated branch label");
}

void test_branch_likely_families() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "beql $a0, $zero, target\n"
        "bnel $a1, $a2, target\n"
        "blezl $s0, target\n"
        "bgtzl $s1, target\n"
        "bltzl $s2, target\n"
        "bgezl $s3, target\n"
        "bltzall $s4, target\n"
        "bgezall $s5, target\n"
        "bc1fl target\n"
        "bc1tl target\n"
        "target: nop\n";
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "R5900 branch-likely families did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == 22U, "R5900 branch-likely word count is incorrect");
    require(words[1] == 0x50800009U, "beql encoding is incorrect");
    require(words[3] == 0x54A60008U, "bnel encoding is incorrect");
    require(words[5] == 0x5A000007U, "blezl encoding is incorrect");
    require(words[7] == 0x5E200006U, "bgtzl encoding is incorrect");
    require(words[9] == 0x06420005U, "bltzl encoding is incorrect");
    require(words[11] == 0x06630004U, "bgezl encoding is incorrect");
    require(words[13] == 0x06920003U, "bltzall encoding is incorrect");
    require(words[15] == 0x06B30002U, "bgezall encoding is incorrect");
    require(words[17] == 0x45020001U, "bc1fl encoding is incorrect");
    require(words[19] == 0x45030000U, "bc1tl encoding is incorrect");

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("beql $a0, $zero, loc_00100028") != std::string::npos,
            "beql was not disassembled with its generated target label");
    require(formatted.find("bnel $a1, $a2, loc_00100028") != std::string::npos,
            "bnel was not disassembled with its generated target label");
    require(formatted.find("blezl $s0, loc_00100028") != std::string::npos,
            "blezl was not disassembled correctly");
    require(formatted.find("bgtzl $s1, loc_00100028") != std::string::npos,
            "bgtzl was not disassembled correctly");
    require(formatted.find("bltzl $s2, loc_00100028") != std::string::npos,
            "bltzl was not disassembled correctly");
    require(formatted.find("bgezl $s3, loc_00100028") != std::string::npos,
            "bgezl was not disassembled correctly");
    require(formatted.find("bltzall $s4, loc_00100028") != std::string::npos,
            "bltzall was not disassembled correctly");
    require(formatted.find("bgezall $s5, loc_00100028") != std::string::npos,
            "bgezall was not disassembled correctly");
    require(formatted.find("bc1fl loc_00100028") != std::string::npos,
            "bc1fl was not disassembled correctly");
    require(formatted.find("bc1tl loc_00100028") != std::string::npos,
            "bc1tl was not disassembled correctly");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "Disassembled branch-likely source did not reassemble");
    require(reparsed.blocks.size() == 1U && reparsed.blocks[0].cheat.words == words,
            "Branch-likely disassembly did not round-trip exactly");
}

void test_mgs3_branch_likely_round_trip() {
    constexpr std::string_view raw =
        "Enable Code (Must Be On)\n"
        "200C0238 8C841D90\n"
        "200C023C 50800008\n"
        "200C0240 AC200000\n"
        "200C0244 8C8536C4\n"
        "200C0248 8C260260\n"
        "200C024C 54A60004\n"
        "200C0250 AC200000\n"
        "200C0254 3C050803\n"
        "200C0258 34A50088\n"
        "200C025C AC8536C4\n"
        "200C0260 03E00008\n"
        "200C0264 00000000\n";

    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result mips = convert::convert_text(raw, to_mips);
    require(mips.output_text.find("beql $a0, $zero, loc_000C0260") != std::string::npos,
            "MGS3 0x50800008 did not decode as beql");
    require(mips.output_text.find("bnel $a1, $a2, loc_000C0260") != std::string::npos,
            "MGS3 0x54A60004 did not decode as bnel");

    convert::Request to_raw;
    to_raw.input_format = CodeFormat::ps2_mips_r5900;
    to_raw.output_format = CodeFormat::standard_raw;
    const convert::Result round_trip = convert::convert_text(mips.output_text, to_raw);
    require(round_trip.output_text.find("200C023C 50800008") != std::string::npos,
            "MGS3 beql did not reassemble to 0x50800008");
    require(round_trip.output_text.find("200C024C 54A60004") != std::string::npos,
            "MGS3 bnel did not reassemble to 0x54A60004");
}


void test_r5900_core_and_cop1_v0193() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "mult $v0, $v0, $v1\n"
        "multu $v0, $v0, $v1\n"
        "dsllv $v1, $v1, $v0\n"
        "dsrlv $v0, $v0, $v1\n"
        "dsrav $v0, $v0, $v1\n"
        "mfsa $v0\n"
        "mtsa $v0\n"
        "mtsab $t5, 0\n"
        "mtsah $zero, 2\n"
        "madd $v0, $v1\n"
        "maddu $v0, $v1\n"
        "madd1 $v0, $v1\n"
        "maddu1 $v0, $v1\n"
        "mult1 $a2, $v1\n"
        "multu1 $a2, $v1\n"
        "madd $v0, $v0, $v1\n"
        "maddu $v0, $v0, $v1\n"
        "madd1 $v0, $v0, $v1\n"
        "maddu1 $v0, $v0, $v1\n"
        "mult1 $v1, $a2, $v1\n"
        "multu1 $v1, $a2, $v1\n"
        "div1 $v0, $v1\n"
        "divu1 $v0, $v1\n"
        "mfhi1 $v0\n"
        "mflo1 $v0\n"
        "mthi1 $v0\n"
        "mtlo1 $v0\n"
        "plzcw $a0, $a0\n"
        "c.lt.s $f0, $f1\n"
        "c.le.s $f1, $f0\n"
        "sqrt.s $f1, $f1\n";

    constexpr std::array<std::uint32_t, 31> expected{{
        0x00431018U, 0x00431019U, 0x00431814U, 0x00621016U, 0x00621017U,
        0x00001028U, 0x00400029U, 0x05B80000U, 0x04190002U,
        0x70430000U, 0x70430001U, 0x70430020U, 0x70430021U,
        0x70C30018U, 0x70C30019U,
        0x70431000U, 0x70431001U, 0x70431020U, 0x70431021U,
        0x70C31818U, 0x70C31819U, 0x7043001AU, 0x7043001BU,
        0x70001010U, 0x70001012U, 0x70400011U, 0x70400013U, 0x70802004U,
        0x46010034U, 0x46000836U, 0x46010044U
    }};

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "v0.19.3 R5900 core/COP1 source did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == expected.size() * 2U,
            "v0.19.3 R5900 core/COP1 word count is incorrect");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(words[(index * 2U) + 1U] == expected[index],
                "v0.19.3 R5900 core/COP1 encoding mismatch at vector " + std::to_string(index));
    }

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("mult $v0, $v0, $v1") != std::string::npos,
            "Three-operand mult was not disassembled canonically");
    require(formatted.find("madd $v0, $v1") != std::string::npos &&
            formatted.find("mult1 $a2, $v1") != std::string::npos,
            "Two-operand dual-HI/LO forms were not disassembled canonically");
    require(formatted.find("multu $v0, $v0, $v1") != std::string::npos,
            "Three-operand multu was not disassembled canonically");
    require(formatted.find("mtsab $t5, 0") != std::string::npos,
            "mtsab was not disassembled correctly");
    require(formatted.find("loc_") == std::string::npos,
            "mtsab/mtsah were incorrectly treated as REGIMM branches");
    require(formatted.find("madd1 $v0, $v0, $v1") != std::string::npos,
            "madd1 was not disassembled correctly");
    require(formatted.find("plzcw $a0, $a0") != std::string::npos,
            "plzcw was not disassembled correctly");
    require(formatted.find("c.lt.s $f0, $f1") != std::string::npos,
            "Correct c.lt.s function code was not disassembled");
    require(formatted.find("c.le.s $f1, $f0") != std::string::npos,
            "Correct c.le.s function code was not disassembled");
    require(formatted.find("sqrt.s $f1, $f1") != std::string::npos,
            "R5900 sqrt.s ft-source layout was not disassembled");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "v0.19.3 R5900 core/COP1 disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "v0.19.3 R5900 core/COP1 vectors did not round-trip exactly");
}

void test_cop0_control_v0193() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "bc0f target\n"
        "bc0t target\n"
        "bc0fl target\n"
        "bc0tl target\n"
        "target: tlbr\n"
        "tlbwi\n"
        "tlbwr\n"
        "tlbp\n"
        "eret\n"
        "ei\n"
        "di\n";

    constexpr std::array<std::uint32_t, 11> expected{{
        0x41000003U, 0x41010002U, 0x41020001U, 0x41030000U,
        0x42000001U, 0x42000002U, 0x42000006U, 0x42000008U,
        0x42000018U, 0x42000038U, 0x42000039U
    }};

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "v0.19.3 COP0 control source did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == expected.size() * 2U,
            "v0.19.3 COP0 control word count is incorrect");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(words[(index * 2U) + 1U] == expected[index],
                "v0.19.3 COP0 encoding mismatch at vector " + std::to_string(index));
    }

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("bc0f loc_00100010") != std::string::npos,
            "bc0f did not use a generated target label");
    require(formatted.find("bc0tl loc_00100010") != std::string::npos,
            "bc0tl did not use a generated target label");
    require(formatted.find("tlbp") != std::string::npos &&
            formatted.find("tlbwr") != std::string::npos &&
            formatted.find("ei") != std::string::npos &&
            formatted.find("di") != std::string::npos,
            "COP0 control instructions were not disassembled correctly");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "v0.19.3 COP0 control disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "v0.19.3 COP0 control vectors did not round-trip exactly");
}

void test_noncanonical_v0192_cop1_words_are_preserved() {
    constexpr std::string_view raw =
        "COP1 old-layout regression\n"
        "20100000 46000844\n"
        "20100004 4601003C\n"
        "20100008 4600083E\n";

    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result mips = convert::convert_text(raw, to_mips);
    require(mips.output_text.find(".word 0x46000844") != std::string::npos,
            "Old sqrt.s fs-source layout should remain an exact .word");
    require(mips.output_text.find(".word 0x4601003C") != std::string::npos,
            "Old c.lt.s function code should remain an exact .word");
    require(mips.output_text.find(".word 0x4600083E") != std::string::npos,
            "Old c.le.s function code should remain an exact .word");
}


void test_mmi_v0220_round_trip() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "paddw $t0, $t1, $t2\n"
        "psubw $t0, $t1, $t2\n"
        "pcgtw $t0, $t1, $t2\n"
        "pmaxw $t0, $t1, $t2\n"
        "paddh $t0, $t1, $t2\n"
        "psubh $t0, $t1, $t2\n"
        "pcgth $t0, $t1, $t2\n"
        "pmaxh $t0, $t1, $t2\n"
        "paddb $t0, $t1, $t2\n"
        "psubb $t0, $t1, $t2\n"
        "pcgtb $t0, $t1, $t2\n"
        "paddsw $t0, $t1, $t2\n"
        "psubsw $t0, $t1, $t2\n"
        "pextlw $t0, $t1, $t2\n"
        "ppacw $t0, $t1, $t2\n"
        "paddsh $t0, $t1, $t2\n"
        "psubsh $t0, $t1, $t2\n"
        "pextlh $t0, $t1, $t2\n"
        "ppach $t0, $t1, $t2\n"
        "paddsb $t0, $t1, $t2\n"
        "psubsb $t0, $t1, $t2\n"
        "pextlb $t0, $t1, $t2\n"
        "ppacb $t0, $t1, $t2\n"
        "pext5 $t0, $t2\n"
        "ppac5 $t0, $t2\n"
        "pabsw $t0, $t2\n"
        "pceqw $t0, $t1, $t2\n"
        "pminw $t0, $t1, $t2\n"
        "padsbh $t0, $t1, $t2\n"
        "pabsh $t0, $t2\n"
        "pceqh $t0, $t1, $t2\n"
        "pminh $t0, $t1, $t2\n"
        "pceqb $t0, $t1, $t2\n"
        "padduw $t0, $t1, $t2\n"
        "psubuw $t0, $t1, $t2\n"
        "pextuw $t0, $t1, $t2\n"
        "padduh $t0, $t1, $t2\n"
        "psubuh $t0, $t1, $t2\n"
        "pextuh $t0, $t1, $t2\n"
        "paddub $t0, $t1, $t2\n"
        "psubub $t0, $t1, $t2\n"
        "pextub $t0, $t1, $t2\n"
        "qfsrv $t0, $t1, $t2\n"
        "pmaddw $t0, $t1, $t2\n"
        "psllvw $t0, $t1, $t2\n"
        "psrlvw $t0, $t1, $t2\n"
        "pmsubw $t0, $t1, $t2\n"
        "pmfhi $t0\n"
        "pmflo $t0\n"
        "pinth $t0, $t1, $t2\n"
        "pmultw $t0, $t1, $t2\n"
        "pdivw $t1, $t2\n"
        "pcpyld $t0, $t1, $t2\n"
        "pmaddh $t0, $t1, $t2\n"
        "phmadh $t0, $t1, $t2\n"
        "pand $t0, $t1, $t2\n"
        "pxor $t0, $t1, $t2\n"
        "pmsubh $t0, $t1, $t2\n"
        "phmsbh $t0, $t1, $t2\n"
        "pexeh $t0, $t2\n"
        "prevh $t0, $t2\n"
        "pmulth $t0, $t1, $t2\n"
        "pdivbw $t1, $t2\n"
        "pexew $t0, $t2\n"
        "prot3w $t0, $t2\n"
        "pmadduw $t0, $t1, $t2\n"
        "psravw $t0, $t1, $t2\n"
        "pmthi $t1\n"
        "pmtlo $t1\n"
        "pinteh $t0, $t1, $t2\n"
        "pmultuw $t0, $t1, $t2\n"
        "pdivuw $t1, $t2\n"
        "pcpyud $t0, $t1, $t2\n"
        "por $t0, $t1, $t2\n"
        "pnor $t0, $t1, $t2\n"
        "pexch $t0, $t2\n"
        "pcpyh $t0, $t2\n"
        "pexcw $t0, $t2\n"
        "pmfhl.lw $t0\n"
        "pmfhl.uw $t0\n"
        "pmfhl.slw $t0\n"
        "pmfhl.lh $t0\n"
        "pmfhl.sh $t0\n"
        "pmthl.lw $t1\n"
        "psllh $t0, $t2, 1\n"
        "psrlh $t0, $t2, 2\n"
        "psrah $t0, $t2, 3\n"
        "psllw $t0, $t2, 4\n"
        "psrlw $t0, $t2, 5\n"
        "psraw $t0, $t2, 6\n";

    constexpr std::array<std::uint32_t, 90> expected{{
        0x712A4008U,
        0x712A4048U,
        0x712A4088U,
        0x712A40C8U,
        0x712A4108U,
        0x712A4148U,
        0x712A4188U,
        0x712A41C8U,
        0x712A4208U,
        0x712A4248U,
        0x712A4288U,
        0x712A4408U,
        0x712A4448U,
        0x712A4488U,
        0x712A44C8U,
        0x712A4508U,
        0x712A4548U,
        0x712A4588U,
        0x712A45C8U,
        0x712A4608U,
        0x712A4648U,
        0x712A4688U,
        0x712A46C8U,
        0x700A4788U,
        0x700A47C8U,
        0x700A4068U,
        0x712A40A8U,
        0x712A40E8U,
        0x712A4128U,
        0x700A4168U,
        0x712A41A8U,
        0x712A41E8U,
        0x712A42A8U,
        0x712A4428U,
        0x712A4468U,
        0x712A44A8U,
        0x712A4528U,
        0x712A4568U,
        0x712A45A8U,
        0x712A4628U,
        0x712A4668U,
        0x712A46A8U,
        0x712A46E8U,
        0x712A4009U,
        0x71494089U,
        0x714940C9U,
        0x712A4109U,
        0x70004209U,
        0x70004249U,
        0x712A4289U,
        0x712A4309U,
        0x712A0349U,
        0x712A4389U,
        0x712A4409U,
        0x712A4449U,
        0x712A4489U,
        0x712A44C9U,
        0x712A4509U,
        0x712A4549U,
        0x700A4689U,
        0x700A46C9U,
        0x712A4709U,
        0x712A0749U,
        0x700A4789U,
        0x700A47C9U,
        0x712A4029U,
        0x714940E9U,
        0x71200229U,
        0x71200269U,
        0x712A42A9U,
        0x712A4329U,
        0x712A0369U,
        0x712A43A9U,
        0x712A44A9U,
        0x712A44E9U,
        0x700A46A9U,
        0x700A46E9U,
        0x700A47A9U,
        0x70004030U,
        0x70004070U,
        0x700040B0U,
        0x700040F0U,
        0x70004130U,
        0x71200031U,
        0x700A4074U,
        0x700A40B6U,
        0x700A40F7U,
        0x700A413CU,
        0x700A417EU,
        0x700A41BFU
    }};

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "v0.22.0 R5900 MMI source did not parse");
    require(parsed.blocks.size() == 1U, "v0.22.0 R5900 MMI block count is incorrect");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == expected.size() * 2U,
            "v0.22.0 R5900 MMI word count is incorrect");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(words[(index * 2U) + 1U] == expected[index],
                "v0.22.0 R5900 MMI encoding mismatch at vector " + std::to_string(index));
    }

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find(".word 0x7") == std::string::npos,
            "Supported MMI opcode was emitted as .word");
    require(formatted.find("paddw $t0, $t1, $t2") != std::string::npos,
            "MMI0 arithmetic was not disassembled");
    require(formatted.find("paddub $t0, $t1, $t2") != std::string::npos,
            "MMI1 unsigned arithmetic was not disassembled");
    require(formatted.find("pmaddw $t0, $t1, $t2") != std::string::npos,
            "MMI2 multiply/accumulate was not disassembled");
    require(formatted.find("psllvw $t0, $t1, $t2") != std::string::npos &&
            formatted.find("psravw $t0, $t1, $t2") != std::string::npos,
            "MMI variable shifts did not preserve canonical rd, rt, rs order");
    require(formatted.find("pmadduw $t0, $t1, $t2") != std::string::npos,
            "MMI3 multiply/accumulate was not disassembled");
    require(formatted.find("pmfhl.sh $t0") != std::string::npos &&
            formatted.find("pmthl.lw $t1") != std::string::npos,
            "PMFHL/PMTHL modes were not disassembled");
    require(formatted.find("psraw $t0, $t2, 6") != std::string::npos,
            "Packed immediate shift was not disassembled");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "v0.22.0 R5900 MMI disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "v0.22.0 R5900 MMI vectors did not round-trip exactly");
}

void test_mmi_shift_ranges_and_noncanonical_preservation_v0220() {
    const auto invalid_halfword = formats::mips_r5900::parse_text(
        ".org 0x00100000\npsllh $t0, $t2, 16\n");
    require(!invalid_halfword.success() &&
            invalid_halfword.errors[0].find("0..15") != std::string::npos,
            "PSLLH accepted a noncanonical shift amount above 15");

    const auto valid_word = formats::mips_r5900::parse_text(
        ".org 0x00100000\npsllw $t0, $t2, 31\n");
    require(valid_word.success() && valid_word.blocks[0].cheat.words[1] == 0x700A47FCU,
            "PSLLW did not accept the full 0..31 shift range");

    constexpr std::string_view raw =
        "Reserved PSLLH high-SA encoding\n"
        "20100000 700A4434\n";
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result result = convert::convert_text(raw, request);
    require(result.output_text.find(".word 0x700A4434") != std::string::npos,
            "Noncanonical PSLLH high-SA encoding was normalized instead of preserved");
}

void test_demo_corpus_mmi_words_v0220() {
    constexpr std::string_view raw =
        "Demo corpus MMI samples\n"
        "20100000 70002E28\n"
        "20100004 73E0FC88\n"
        "20100008 700006E8\n"
        "2010000C 70000030\n"
        "20100010 700A4074\n";

    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result mips = convert::convert_text(raw, to_mips);
    require(mips.output_text.find("paddub $a1, $zero, $zero") != std::string::npos,
            "Demo corpus PADDUB word was not decoded");
    require(mips.output_text.find("pextlw $ra, $ra, $zero") != std::string::npos,
            "GNU R5900 PEXTLW vector was not decoded");
    require(mips.output_text.find("qfsrv $zero, $zero, $zero") != std::string::npos,
            "Demo corpus QFSRV word was not decoded");
    require(mips.output_text.find("pmfhl.lw $zero") != std::string::npos,
            "PMFHL.LW word was not decoded");
    require(mips.output_text.find("psllh $t0, $t2, 1") != std::string::npos,
            "PSLLH word was not decoded");

    convert::Request to_raw;
    to_raw.input_format = CodeFormat::ps2_mips_r5900;
    to_raw.output_format = CodeFormat::standard_raw;
    const convert::Result round_trip = convert::convert_text(mips.output_text, to_raw);
    require(round_trip.output_text.find("20100000 70002E28") != std::string::npos &&
            round_trip.output_text.find("20100004 73E0FC88") != std::string::npos &&
            round_trip.output_text.find("20100010 700A4074") != std::string::npos,
            "Demo corpus MMI words did not round-trip exactly");
}

void test_traps_and_r5900_cop1_v0230_round_trip() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "tge $t0, $t1\n"
        "tgeu $t0, $t1\n"
        "tlt $t0, $t1\n"
        "tltu $t0, $t1\n"
        "teq $t0, $t1\n"
        "tne $t0, $t1, 0x155\n"
        "tgei $t0, -123\n"
        "tgeiu $t0, -123\n"
        "tlti $t0, -123\n"
        "tltiu $t0, -123\n"
        "teqi $t0, -123\n"
        "tnei $t0, -123\n"
        "rsqrt.s $f1, $f2, $f3\n"
        "adda.s $f2, $f3\n"
        "suba.s $f2, $f3\n"
        "mula.s $f2, $f3\n"
        "madd.s $f1, $f2, $f3\n"
        "msub.s $f1, $f2, $f3\n"
        "madda.s $f2, $f3\n"
        "msuba.s $f2, $f3\n"
        "max.s $f1, $f2, $f3\n"
        "min.s $f1, $f2, $f3\n"
        "c.f.s $f2, $f3\n";

    constexpr std::array<std::uint32_t, 23> expected{{
        0x01090030U, 0x01090031U, 0x01090032U, 0x01090033U,
        0x01090034U, 0x01095576U,
        0x0508FF85U, 0x0509FF85U, 0x050AFF85U, 0x050BFF85U,
        0x050CFF85U, 0x050EFF85U,
        0x46031056U, 0x46031018U, 0x46031019U, 0x4603101AU,
        0x4603105CU, 0x4603105DU, 0x4603101EU, 0x4603101FU,
        0x46031068U, 0x46031069U, 0x46031030U
    }};

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "v0.23.0 trap/COP1 source did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == expected.size() * 2U,
            "v0.23.0 trap/COP1 word count is incorrect");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(words[(index * 2U) + 1U] == expected[index],
                "v0.23.0 trap/COP1 encoding mismatch at vector " + std::to_string(index));
    }

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("tge $t0, $t1") != std::string::npos &&
            formatted.find("tne $t0, $t1, 0x155") != std::string::npos,
            "Register trap instructions were not disassembled canonically");
    require(formatted.find("tgei $t0, -123") != std::string::npos &&
            formatted.find("tnei $t0, -123") != std::string::npos,
            "Immediate trap instructions were not disassembled canonically");
    require(formatted.find("rsqrt.s $f1, $f2, $f3") != std::string::npos &&
            formatted.find("madd.s $f1, $f2, $f3") != std::string::npos,
            "Three-register R5900 COP1 instructions were not disassembled");
    require(formatted.find("adda.s $f2, $f3") != std::string::npos &&
            formatted.find("msuba.s $f2, $f3") != std::string::npos,
            "R5900 accumulator COP1 instructions were not disassembled");
    require(formatted.find("max.s $f1, $f2, $f3") != std::string::npos &&
            formatted.find("c.f.s $f2, $f3") != std::string::npos,
            "R5900 min/max/compare COP1 instructions were not disassembled");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "v0.23.0 trap/COP1 disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "v0.23.0 trap/COP1 vectors did not round-trip exactly");
}

void test_trap_ranges_and_noncanonical_cop1_preservation_v0230() {
    const auto invalid_code = formats::mips_r5900::parse_text(
        ".org 0x00100000\ntge $t0, $t1, 0x400\n");
    require(!invalid_code.success() &&
            invalid_code.errors[0].find("0..0x3FF") != std::string::npos,
            "Register trap accepted a code above 10 bits");

    const auto invalid_immediate = formats::mips_r5900::parse_text(
        ".org 0x00100000\ntgei $t0, 0x10000\n");
    require(!invalid_immediate.success() &&
            invalid_immediate.errors[0].find("16 bits") != std::string::npos,
            "Immediate trap accepted a value above 16 bits");

    constexpr std::string_view raw =
        "Reserved R5900 COP1 encodings\n"
        "20100000 46031058\n"
        "20100004 46031070\n";
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result result = convert::convert_text(raw, request);
    require(result.output_text.find(".word 0x46031058") != std::string::npos,
            "Noncanonical ADDA.S destination field was normalized instead of preserved");
    require(result.output_text.find(".word 0x46031070") != std::string::npos,
            "Noncanonical C.F.S condition field was normalized instead of preserved");
}

void test_demo_corpus_r5900_cop1_words_v0230() {
    constexpr std::string_view raw =
        "Demo corpus R5900 COP1 samples\n"
        "2013F658 46080216\n"
        "20100118 46010018\n"
        "2017EE88 46011019\n"
        "2013F648 4604201A\n"
        "2013F654 46073A1C\n"
        "20133D2C 4604311D\n"
        "203A4974 4602081E\n"
        "2017CF38 4601281F\n"
        "20101014 46150868\n"
        "20101024 46140869\n"
        "210A9170 46070030\n";

    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result mips = convert::convert_text(raw, to_mips);
    require(mips.output_text.find("rsqrt.s $f8, $f0, $f8") != std::string::npos,
            "Anubis demo RSQRT.S word was not decoded");
    require(mips.output_text.find("adda.s $f0, $f1") != std::string::npos &&
            mips.output_text.find("suba.s $f2, $f1") != std::string::npos &&
            mips.output_text.find("mula.s $f4, $f4") != std::string::npos,
            "Demo accumulator arithmetic words were not decoded");
    require(mips.output_text.find("madd.s $f8, $f7, $f7") != std::string::npos &&
            mips.output_text.find("msub.s $f4, $f6, $f4") != std::string::npos,
            "Demo multiply-add/subtract words were not decoded");
    require(mips.output_text.find("madda.s $f1, $f2") != std::string::npos &&
            mips.output_text.find("msuba.s $f5, $f1") != std::string::npos,
            "Demo accumulator multiply-add/subtract words were not decoded");
    require(mips.output_text.find("max.s $f1, $f1, $f21") != std::string::npos &&
            mips.output_text.find("min.s $f1, $f1, $f20") != std::string::npos,
            "Demo MIN.S/MAX.S words were not decoded");
    require(mips.output_text.find("c.f.s $f0, $f7") != std::string::npos,
            "Destroy All Humans! 2 demo C.F.S word was not decoded");

    convert::Request to_raw;
    to_raw.input_format = CodeFormat::ps2_mips_r5900;
    to_raw.output_format = CodeFormat::standard_raw;
    const convert::Result round_trip = convert::convert_text(mips.output_text, to_raw);
    require(round_trip.output_text.find("2013F658 46080216") != std::string::npos &&
            round_trip.output_text.find("2013F654 46073A1C") != std::string::npos &&
            round_trip.output_text.find("210A9170 46070030") != std::string::npos,
            "Demo R5900 COP1 words did not round-trip exactly");
}


void test_cop2_transfers_and_branches_v0240_round_trip() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "qmfc2 $t0, $vf1\n"
        "qmfc2.i $t0, $vf1\n"
        "qmfc2.ni $t0, $vf1\n"
        "qmtc2 $t1, $vf2\n"
        "cfc2 $v0, $vi16\n"
        "ctc2.i $v1, $vi17\n"
        "bc2f target\n"
        "bc2t target\n"
        "bc2fl target\n"
        "bc2tl target\n"
        "target: nop\n";

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "v0.24.0 COP2 transfer/branch vectors did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == 22U, "v0.24.0 COP2 vector count is incorrect");
    require(words[1] == 0x48280800U, "QMFC2 encoding is incorrect");
    require(words[3] == 0x48280801U, "QMFC2.I encoding is incorrect");
    require(words[5] == 0x48280800U, "QMFC2.NI alias encoding is incorrect");
    require(words[7] == 0x48A91000U, "QMTC2 encoding is incorrect");
    require(words[9] == 0x48428000U, "CFC2 encoding is incorrect");
    require(words[11] == 0x48C38801U, "CTC2.I encoding is incorrect");
    require(words[13] == 0x49000003U, "BC2F encoding is incorrect");
    require(words[15] == 0x49010002U, "BC2T encoding is incorrect");
    require(words[17] == 0x49020001U, "BC2FL encoding is incorrect");
    require(words[19] == 0x49030000U, "BC2TL encoding is incorrect");

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("qmfc2 $t0, $vf1") != std::string::npos,
            "QMFC2 was not disassembled correctly");
    require(formatted.find("qmfc2.i $t0, $vf1") != std::string::npos,
            "QMFC2.I was not disassembled correctly");
    require(formatted.find("qmtc2 $t1, $vf2") != std::string::npos,
            "QMTC2 was not disassembled correctly");
    require(formatted.find("cfc2 $v0, $vi16") != std::string::npos,
            "CFC2 was not disassembled correctly");
    require(formatted.find("ctc2.i $v1, $vi17") != std::string::npos,
            "CTC2.I was not disassembled correctly");
    require(formatted.find("bc2f loc_00100028") != std::string::npos &&
            formatted.find("bc2tl loc_00100028") != std::string::npos,
            "COP2 branches did not use the generated target label");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "v0.24.0 COP2 disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "v0.24.0 COP2 transfer/branch vectors did not round-trip exactly");
}

void test_vu0_core_macro_v0240_round_trip() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "vadd.xyzw $vf1, $vf2, $vf3\n"
        "vadd.x $vf1, $vf2, $vf3\n"
        "vaddy.zw $vf4, $vf5, $vf6\n"
        "vmulw.y $vf7, $vf8, $vf9\n"
        "vmini.xz $vf10, $vf11, $vf12\n"
        "vadd $vf1, $vf2, $vf3\n"
        "vsub $vf1, $vf2, $vf3\n"
        "vmul $vf1, $vf2, $vf3\n"
        "vmadd $vf1, $vf2, $vf3\n"
        "vmsub $vf1, $vf2, $vf3\n"
        "vmax $vf1, $vf2, $vf3\n"
        "vmini $vf1, $vf2, $vf3\n";

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "v0.24.0 VU0 core macro vectors did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == 24U, "v0.24.0 VU0 core vector count is incorrect");
    require(words[1] == 0x4BE31068U, "VADD.XYZW encoding is incorrect");
    require(words[3] == 0x4B031068U, "VADD.X destination mask encoding is incorrect");
    require(words[5] == 0x4A662901U, "VADDY.ZW broadcast encoding is incorrect");
    require(words[7] == 0x4A8941DBU, "VMULW.Y broadcast encoding is incorrect");
    require(words[9] == 0x4B4C5AAFU, "VMINI.XZ encoding is incorrect");
    require(words[11] == 0x4BE31068U, "VU0 no-suffix XYZW default is incorrect");
    require(words[13] == 0x4BE3106CU, "VSUB encoding is incorrect");
    require(words[15] == 0x4BE3106AU, "VMUL encoding is incorrect");
    require(words[17] == 0x4BE31069U, "VMADD encoding is incorrect");
    require(words[19] == 0x4BE3106DU, "VMSUB encoding is incorrect");
    require(words[21] == 0x4BE3106BU, "VMAX encoding is incorrect");
    require(words[23] == 0x4BE3106FU, "VMINI encoding is incorrect");

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("vadd.xyzw $vf1, $vf2, $vf3") != std::string::npos,
            "VADD.XYZW was not disassembled correctly");
    require(formatted.find("vaddy.zw $vf4, $vf5, $vf6") != std::string::npos,
            "VADDY.ZW was not disassembled correctly");
    require(formatted.find("vmulw.y $vf7, $vf8, $vf9") != std::string::npos,
            "VMULW.Y was not disassembled correctly");
    require(formatted.find("vmini.xz $vf10, $vf11, $vf12") != std::string::npos,
            "VMINI.XZ was not disassembled correctly");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "v0.24.0 VU0 macro disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "v0.24.0 VU0 macro vectors did not round-trip exactly");
}

void test_vu0_all_core_functions_v0240_round_trip() {
    constexpr std::array<std::string_view, 35> mnemonics{{
        "vaddx", "vaddy", "vaddz", "vaddw",
        "vsubx", "vsuby", "vsubz", "vsubw",
        "vmaddx", "vmaddy", "vmaddz", "vmaddw",
        "vmsubx", "vmsuby", "vmsubz", "vmsubw",
        "vmaxx", "vmaxy", "vmaxz", "vmaxw",
        "vminix", "vminiy", "vminiz", "vminiw",
        "vmulx", "vmuly", "vmulz", "vmulw",
        "vadd", "vmadd", "vmul", "vmax", "vsub", "vmsub", "vmini"
    }};

    std::string source = ".org 0x00100000\n";
    for (const std::string_view mnemonic : mnemonics) {
        source += std::string(mnemonic) + ".xyzw $vf1, $vf2, $vf3\n";
    }
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "One or more v0.24.0 VU0 core functions did not assemble");
    require(parsed.blocks[0].cheat.words.size() == mnemonics.size() * 2U,
            "v0.24.0 VU0 complete core function count is incorrect");

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "Complete v0.24.0 VU0 core function output did not reassemble");
    require(reparsed.blocks[0].cheat.words == parsed.blocks[0].cheat.words,
            "Complete v0.24.0 VU0 core function set did not round-trip exactly");
}

void test_cop2_vu0_noncanonical_preservation_v0240() {
    constexpr std::string_view raw =
        "Reserved COP2/VU0 encodings\n"
        "20100000 48280802\n"
        "20100004 4A031068\n";
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result result = convert::convert_text(raw, request);
    require(result.output_text.find(".word 0x48280802") != std::string::npos,
            "Reserved QMFC2 low bits were normalized instead of preserved");
    require(result.output_text.find(".word 0x4A031068") != std::string::npos,
            "VU0 macro encoding with an empty destination mask was normalized instead of preserved");

    const auto empty_suffix = formats::mips_r5900::parse_text(
        ".org 0x00100000\nvadd. $vf1, $vf2, $vf3\n");
    require(!empty_suffix.success() &&
            empty_suffix.errors[0].find("cannot be empty") != std::string::npos,
            "VU0 accepted an empty destination suffix");

    const auto invalid_suffix = formats::mips_r5900::parse_text(
        ".org 0x00100000\nvadd.q $vf1, $vf2, $vf3\n");
    require(!invalid_suffix.success() &&
            invalid_suffix.errors[0].find("expected x, y, z, and/or w") != std::string::npos,
            "VU0 accepted an invalid destination component");

    const auto duplicate_suffix = formats::mips_r5900::parse_text(
        ".org 0x00100000\nvadd.xx $vf1, $vf2, $vf3\n");
    require(!duplicate_suffix.success() &&
            duplicate_suffix.errors[0].find("duplicate") != std::string::npos,
            "VU0 accepted a duplicate destination component");
}



void test_official_v6_top_level_opcode_audit_v0250() {
    constexpr std::string_view raw =
        "Official v6 unsupported top-level opcodes\n"
        "20100000 C1280000\n" // LL
        "20100004 C9280000\n" // LWC2
        "20100008 D1280000\n" // LLD
        "2010000C D5280000\n" // LDC1
        "20100010 E1280000\n" // SC
        "20100014 E9280000\n" // SWC2
        "20100018 F1280000\n" // SCD
        "2010001C F5280000\n"; // SDC1

    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result result = convert::convert_text(raw, request);
    constexpr std::array<std::string_view, 8> words{{
        "C1280000", "C9280000", "D1280000", "D5280000",
        "E1280000", "E9280000", "F1280000", "F5280000"
    }};
    for (const std::string_view word : words) {
        require(result.output_text.find(".word 0x" + std::string(word)) != std::string::npos,
                "Unsupported R5900 top-level opcode 0x" + std::string(word) +
                " was decoded as a generic MIPS instruction");
    }

    constexpr std::array<std::string_view, 8> unsupported_sources{{
        ".org 0x00100000\nll $t0, 0($t1)\n",
        ".org 0x00100000\nlwc2 $vf8, 0($t1)\n",
        ".org 0x00100000\nlld $t0, 0($t1)\n",
        ".org 0x00100000\nldc1 $f8, 0($t1)\n",
        ".org 0x00100000\nsc $t0, 0($t1)\n",
        ".org 0x00100000\nswc2 $vf8, 0($t1)\n",
        ".org 0x00100000\nscd $t0, 0($t1)\n",
        ".org 0x00100000\nsdc1 $f8, 0($t1)\n"
    }};
    for (const std::string_view source : unsupported_sources) {
        const auto parsed = formats::mips_r5900::parse_text(source);
        require(!parsed.success() &&
                parsed.errors[0].find("unsupported R5900 mnemonic") != std::string::npos,
                "Officially unsupported R5900 opcode was accepted by the assembler");
    }
}

void test_official_v6_cop0_debug_performance_v0250() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "mfbpc $t0\n"
        "mfiab $t0\n"
        "mfiabm $t0\n"
        "mfdab $t0\n"
        "mfdabm $t0\n"
        "mfdvb $t0\n"
        "mfdvbm $t0\n"
        "mfpc $t0, 0\n"
        "mfpc $t0, 1\n"
        "mfps $t0, 0\n"
        "mtbpc $t1\n"
        "mtiab $t1\n"
        "mtiabm $t1\n"
        "mtdab $t1\n"
        "mtdabm $t1\n"
        "mtdvb $t1\n"
        "mtdvbm $t1\n"
        "mtpc $t1, 0\n"
        "mtpc $t1, 1\n"
        "mtps $t1, 0\n";

    constexpr std::array<std::uint32_t, 20> expected{{
        0x4008C000U, 0x4008C002U, 0x4008C003U, 0x4008C004U,
        0x4008C005U, 0x4008C006U, 0x4008C007U, 0x4008C801U,
        0x4008C803U, 0x4008C800U, 0x4089C000U, 0x4089C002U,
        0x4089C003U, 0x4089C004U, 0x4089C005U, 0x4089C006U,
        0x4089C007U, 0x4089C801U, 0x4089C803U, 0x4089C800U
    }};

    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "Official v6 COP0 debug/performance instructions did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    require(words.size() == expected.size() * 2U,
            "Official v6 COP0 debug/performance word count is incorrect");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(words[(index * 2U) + 1U] == expected[index],
                "Official v6 COP0 encoding mismatch at vector " + std::to_string(index));
    }

    const std::string formatted = formats::mips_r5900::write_text(parsed.blocks);
    require(formatted.find("mfbpc $t0") != std::string::npos &&
            formatted.find("mfdvbm $t0") != std::string::npos &&
            formatted.find("mfpc $t0, 1") != std::string::npos &&
            formatted.find("mfps $t0, 0") != std::string::npos,
            "COP0 move-from debug/performance instructions were not disassembled canonically");
    require(formatted.find("mtbpc $t1") != std::string::npos &&
            formatted.find("mtdvbm $t1") != std::string::npos &&
            formatted.find("mtpc $t1, 1") != std::string::npos &&
            formatted.find("mtps $t1, 0") != std::string::npos,
            "COP0 move-to debug/performance instructions were not disassembled canonically");

    const auto reparsed = formats::mips_r5900::parse_text(formatted);
    require(reparsed.success(), "Official v6 COP0 disassembly did not reassemble");
    require(reparsed.blocks[0].cheat.words == words,
            "Official v6 COP0 debug/performance instructions did not round-trip exactly");

    const auto bad_counter = formats::mips_r5900::parse_text(
        ".org 0x00100000\nmfpc $t0, 2\n");
    require(!bad_counter.success() && bad_counter.errors[0].find("0 or 1") != std::string::npos,
            "MFPC accepted an invalid performance-counter number");
    const auto bad_specifier = formats::mips_r5900::parse_text(
        ".org 0x00100000\nmtps $t0, 1\n");
    require(!bad_specifier.success() && bad_specifier.errors[0].find("must be 0") != std::string::npos,
            "MTPS accepted an invalid performance-event register number");
    const auto generic_select = formats::mips_r5900::parse_text(
        ".org 0x00100000\nmfc0 $t0, $c12, 1\n");
    require(!generic_select.success() && generic_select.errors[0].find("expects 2 operand") != std::string::npos,
            "R5900 MFC0 incorrectly accepted a generic MIPS select operand");

    constexpr std::string_view reserved =
        "Reserved COP0 encodings\n"
        "20100000 4008C001\n" // Unassigned debug-register low code.
        "20100004 40086001\n" // Generic MFC0 with forbidden select bits.
        "20100008 4008C805\n"; // Performance counter register 2.
    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result reserved_result = convert::convert_text(reserved, to_mips);
    require(reserved_result.output_text.find(".word 0x4008C001") != std::string::npos &&
            reserved_result.output_text.find(".word 0x40086001") != std::string::npos &&
            reserved_result.output_text.find(".word 0x4008C805") != std::string::npos,
            "Reserved COP0 encodings were normalized instead of preserved");
}

void test_official_v6_cop1_control_restrictions_v0250() {
    constexpr std::string_view source =
        ".org 0x00100000\n"
        "cfc1 $t0, $f0\n"
        "cfc1 $t1, $f31\n"
        "ctc1 $v0, $f31\n";
    constexpr std::array<std::uint32_t, 3> expected{{
        0x44480000U, 0x4449F800U, 0x44C2F800U
    }};
    const auto parsed = formats::mips_r5900::parse_text(source);
    require(parsed.success(), "Valid official v6 CFC1/CTC1 control registers did not parse");
    const auto& words = parsed.blocks[0].cheat.words;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(words[(index * 2U) + 1U] == expected[index],
                "Official v6 CFC1/CTC1 encoding mismatch at vector " + std::to_string(index));
    }

    const auto bad_cfc1 = formats::mips_r5900::parse_text(
        ".org 0x00100000\ncfc1 $t0, $f1\n");
    require(!bad_cfc1.success() && bad_cfc1.errors[0].find("$f0 and $f31") != std::string::npos,
            "CFC1 accepted an undefined R5900 FPU control register");
    const auto bad_ctc1 = formats::mips_r5900::parse_text(
        ".org 0x00100000\nctc1 $t0, $f0\n");
    require(!bad_ctc1.success() && bad_ctc1.errors[0].find("$f31") != std::string::npos,
            "CTC1 accepted an undefined R5900 FPU control register");

    constexpr std::string_view raw =
        "Undefined COP1 control-register encodings\n"
        "20100000 44480800\n"
        "20100004 44C80000\n";
    convert::Request request;
    request.input_format = CodeFormat::standard_raw;
    request.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result result = convert::convert_text(raw, request);
    require(result.output_text.find(".word 0x44480800") != std::string::npos &&
            result.output_text.find(".word 0x44C80000") != std::string::npos,
            "Undefined CFC1/CTC1 control-register encodings were normalized");
}

void test_official_v6_jump_region_boundary_v0250() {
    const auto jump = formats::mips_r5900::parse_text(
        ".org 0x0FFFFFFC\nj 0x10000000\n");
    require(jump.success(), "J did not use PC+4 for the 256 MiB region boundary");
    require(jump.blocks[0].cheat.words[1] == 0x08000000U,
            "J region-boundary encoding is incorrect");
    const std::string jump_formatted = formats::mips_r5900::write_text(jump.blocks);
    require(jump_formatted.find("j 0x10000000") != std::string::npos,
            "J region-boundary target was reconstructed from PC instead of PC+4");

    const auto jump_and_link = formats::mips_r5900::parse_text(
        ".org 0x0FFFFFFC\njal 0x10000004\n");
    require(jump_and_link.success(), "JAL did not use PC+4 for the 256 MiB region boundary");
    require(jump_and_link.blocks[0].cheat.words[1] == 0x0C000001U,
            "JAL region-boundary encoding is incorrect");
    const std::string jal_formatted = formats::mips_r5900::write_text(jump_and_link.blocks);
    require(jal_formatted.find("jal 0x10000004") != std::string::npos,
            "JAL region-boundary target was reconstructed from PC instead of PC+4");

    const auto wrong_region = formats::mips_r5900::parse_text(
        ".org 0x0FFFFFFC\nj 0x00000000\n");
    require(!wrong_region.success() && wrong_region.errors[0].find("outside the current 256 MiB region") != std::string::npos,
            "J incorrectly accepted a target outside the PC+4 region");
}

void test_demo_corpus_cop2_vu0_words_v0240() {
    constexpr std::string_view raw =
        "0 Story trial COP2/VU0 samples\n"
        "20223A54 48A83000\n"
        "20223960 4A2631AC\n"
        "20223978 4BC5216A\n"
        "20223984 48222800\n"
        "20223AF8 4BE521A8\n"
        "20223B44 4BE52198\n"
        "20223980 4B052942\n"
        "2022397C 4B052941\n";

    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result mips = convert::convert_text(raw, to_mips);
    require(mips.output_text.find("qmtc2 $t0, $vf6") != std::string::npos &&
            mips.output_text.find("qmfc2 $v0, $vf5") != std::string::npos,
            "0 Story demo COP2 transfer words were not decoded");
    require(mips.output_text.find("vsub.w $vf6, $vf6, $vf6") != std::string::npos &&
            mips.output_text.find("vmul.xyz $vf5, $vf4, $vf5") != std::string::npos,
            "0 Story demo VU0 arithmetic words were not decoded");
    require(mips.output_text.find("vadd.xyzw $vf6, $vf4, $vf5") != std::string::npos &&
            mips.output_text.find("vmulx.xyzw $vf6, $vf4, $vf5") != std::string::npos,
            "0 Story demo VU0 full-mask words were not decoded");
    require(mips.output_text.find("vaddz.x $vf5, $vf5, $vf5") != std::string::npos &&
            mips.output_text.find("vaddy.x $vf5, $vf5, $vf5") != std::string::npos,
            "0 Story demo VU0 broadcast words were not decoded");

    convert::Request to_raw;
    to_raw.input_format = CodeFormat::ps2_mips_r5900;
    to_raw.output_format = CodeFormat::standard_raw;
    const convert::Result round_trip = convert::convert_text(mips.output_text, to_raw);
    require(round_trip.output_text.find("20223A54 48A83000") != std::string::npos &&
            round_trip.output_text.find("20223960 4A2631AC") != std::string::npos &&
            round_trip.output_text.find("2022397C 4B052941") != std::string::npos,
            "Demo COP2/VU0 words did not round-trip exactly");
}

void test_conversion_service_routing() {
    constexpr std::string_view source =
        "Cave\n"
        ".org 0x000A0000\n"
        "lui $at, 0x447A\n"
        "jr $ra\n"
        "nop\n";
    convert::Request to_raw;
    to_raw.input_format = CodeFormat::ps2_mips_r5900;
    to_raw.output_format = CodeFormat::standard_raw;
    const convert::Result raw = convert::convert_text(source, to_raw);
    require(raw.output_text.find("200A0000 3C01447A") != std::string::npos,
            "MIPS input did not route through the assembler");

    convert::Request to_mips;
    to_mips.input_format = CodeFormat::standard_raw;
    to_mips.output_format = CodeFormat::ps2_mips_r5900;
    const convert::Result mips = convert::convert_text(raw.output_text, to_mips);
    require(mips.output_text.find("lui $at, 0x447A") != std::string::npos,
            "MIPS output did not route through the disassembler");
    require(mips.output_text.find("jr $ra") != std::string::npos,
            "MIPS output lost the return instruction");
}

void test_parse_errors_are_reported() {
    convert::Request request;
    request.input_format = CodeFormat::ps2_mips_r5900;
    request.output_format = CodeFormat::standard_raw;
    bool threw = false;
    try {
        (void)convert::convert_text(".org 0x000A0000\naddiu $v0, $v0\n", request);
    } catch (const std::invalid_argument& error) {
        threw = std::string(error.what()).find("expects 3 operand") != std::string::npos;
    }
    require(threw, "MIPS parse/assembly errors were not surfaced clearly");
}

} // namespace

void run_mips_r5900_tests() {
    test_format_identity();
    test_basic_code_cave();
    test_hook_shorthand();
    test_data_widths_and_raw_code();
    test_branch_labels_and_families();
    test_branch_likely_families();
    test_mgs3_branch_likely_round_trip();
    test_r5900_core_and_cop1_v0193();
    test_cop0_control_v0193();
    test_noncanonical_v0192_cop1_words_are_preserved();
    test_mmi_v0220_round_trip();
    test_mmi_shift_ranges_and_noncanonical_preservation_v0220();
    test_demo_corpus_mmi_words_v0220();
    test_traps_and_r5900_cop1_v0230_round_trip();
    test_trap_ranges_and_noncanonical_cop1_preservation_v0230();
    test_demo_corpus_r5900_cop1_words_v0230();
    test_cop2_transfers_and_branches_v0240_round_trip();
    test_vu0_core_macro_v0240_round_trip();
    test_vu0_all_core_functions_v0240_round_trip();
    test_cop2_vu0_noncanonical_preservation_v0240();
    test_official_v6_top_level_opcode_audit_v0250();
    test_official_v6_cop0_debug_performance_v0250();
    test_official_v6_cop1_control_restrictions_v0250();
    test_official_v6_jump_region_boundary_v0250();
    test_demo_corpus_cop2_vu0_words_v0240();
    test_conversion_service_routing();
    test_parse_errors_are_reported();
}

} // namespace omni::tests
