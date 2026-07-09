#include "tests/test_copy_paste_cleanup.hpp"

#include "formats/text/copy_paste_cleaner.hpp"
#include "tests/test_support.hpp"

#include <string>

namespace omni::tests {

void run_copy_paste_cleanup_tests() {
    using formats::text::clean_copy_paste_text;

    const std::string codebreaker_site =
        "With this game, you MUST have Code Breaker Version 6 or higher.\n"
        "1EEnable Code (Must Be On)9A242B73 18F711F9\n"
        "9AAC5E06 187B06D5\n"
        "Action Mission Codes\n"
        "1Never ReloadDA95C636 A2B2BC38\n"
        "2A732F81 00000000\n"
        "2Infinite AmmoDA95C636 A2B2BC38\n"
        "2ACB2798 00000000\n"
        "3Extra AmmoDA95C636 A2B2BC38\n"
        "2AC3279F 00451021\n";

    const std::string expected_codebreaker =
        "With this game, you MUST have Code Breaker Version 6 or higher.\r\n"
        "1EEnable Code (Must Be On)\r\n"
        "9A242B73 18F711F9\r\n"
        "9AAC5E06 187B06D5\r\n"
        "Action Mission Codes\r\n"
        "1Never Reload\r\n"
        "DA95C636 A2B2BC38\r\n"
        "2A732F81 00000000\r\n"
        "2Infinite Ammo\r\n"
        "DA95C636 A2B2BC38\r\n"
        "2ACB2798 00000000\r\n"
        "3Extra Ammo\r\n"
        "DA95C636 A2B2BC38\r\n"
        "2AC3279F 00451021";
    require(clean_copy_paste_text(codebreaker_site) == expected_codebreaker,
            "CodeBreaker-site glued label/code cleanup failed");

    const std::string gamehacking =
        "Master Codes\n"
        " Enable Code (Must Be On) [Read Note] by Lajos Szalay\n"
        "Codebreaker v7+\n"
        "904903A8 0C109375\n"
        "2012EF74 00441021\n"
        "Activate Cheat Codes \n"
        " P1 Press L2+Left Display Factions Enable by Lajos Szalay\n"
        "Codebreaker v7+\n"
        "E005FE7F 00A83D1C\n"
        "205CC35C 00000001\n"
        " Venezuela is Always Friendly by BigBossman\n"
        "Codebreaker v7+\n"
        "205C84D4 3F800000\n";

    const std::string expected_gamehacking =
        "Master Codes\r\n"
        "Enable Code (Must Be On) [Read Note] , by Lajos Szalay , Crypt Codebreaker v7+\r\n"
        "904903A8 0C109375\r\n"
        "2012EF74 00441021\r\n"
        "Activate Cheat Codes\r\n"
        "P1 Press L2+Left Display Factions Enable , by Lajos Szalay , Crypt Codebreaker v7+\r\n"
        "E005FE7F 00A83D1C\r\n"
        "205CC35C 00000001\r\n"
        "Venezuela is Always Friendly , by BigBossman , Crypt Codebreaker v7+\r\n"
        "205C84D4 3F800000";
    const std::string cleaned_gamehacking = clean_copy_paste_text(gamehacking);
    require(cleaned_gamehacking == expected_gamehacking,
            "GameHacking.org author/crypt cleanup failed");
    require(clean_copy_paste_text(cleaned_gamehacking) == cleaned_gamehacking,
            "copy/paste cleanup must be idempotent");

    const std::string cmp =
        "%Credits: Tester\n"
        "+Infinite Health\n"
        "20123456 00000001\n"
        "+$500 Granted\n"
        "20123458 00000002\n";
    const std::string expected_cmp =
        "Infinite Health by Tester\r\n"
        "20123456 00000001\r\n"
        "+$500 Granted\r\n"
        "20123458 00000002";
    require(clean_copy_paste_text(cmp) == expected_cmp,
            "CMP credits or escaped-label cleanup regressed");

    const std::string nested_cmp =
        "!City Codes:\n"
        "!An Ding Codes:\n"
        "+Max Current Defense\n"
        "$20123456 00000001\n"
        "!!\n"
        "!!\n";
    const std::string expected_nested_cmp =
        "!City Codes:\r\n"
        "!An Ding Codes:\r\n"
        "Max Current Defense\r\n"
        "20123456 00000001\r\n"
        "!!\r\n"
        "!!";
    require(clean_copy_paste_text(nested_cmp) == expected_nested_cmp,
            "CMP explicit subgroup markers were not preserved by paste/load cleanup");
}

} // namespace omni::tests
