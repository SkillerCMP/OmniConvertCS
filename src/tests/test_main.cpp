#include "omni/identity.hpp"

#include "tests/test_action_replay.hpp"
#include "tests/test_armax.hpp"
#include "tests/test_armax_disc_hash.hpp"
#include "tests/test_armax_translation.hpp"
#include "tests/test_core.hpp"
#include "tests/test_copy_paste_cleanup.hpp"
#include "tests/test_cmp_output_mode.hpp"
#include "tests/test_format_aliases.hpp"
#include "tests/test_codebreaker.hpp"
#include "tests/test_gameshark.hpp"
#include "tests/test_vector_inventory.hpp"
#include "tests/test_translation.hpp"
#include "tests/test_cb_gs_translation.hpp"
#include "tests/test_transpose_modes.hpp"
#include "tests/test_pnach.hpp"
#include "tests/test_pnach_crc.hpp"
#include "tests/test_inline_mode.hpp"
#include "tests/test_mips_r5900.hpp"
#include "tests/test_binary_formats.hpp"
#include "tests/test_options.hpp"
#include "tests/test_analysis.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        omni::tests::run_core_tests();
        omni::tests::run_copy_paste_cleanup_tests();
        omni::tests::run_cmp_output_mode_tests();
        omni::tests::run_format_alias_tests();
        omni::tests::run_action_replay_tests();
        omni::tests::run_armax_tests();
        omni::tests::run_armax_disc_hash_tests();
        omni::tests::run_codebreaker_tests();
        omni::tests::run_gameshark_tests();
        omni::tests::run_vector_inventory_tests();
        omni::tests::run_translation_tests();
        omni::tests::run_armax_translation_tests();
        omni::tests::run_cb_gs_translation_tests();
        omni::tests::run_transpose_mode_tests();
        omni::tests::run_pnach_tests();
        omni::tests::run_pnach_crc_tests();
        omni::tests::run_inline_mode_tests();
        omni::tests::run_mips_r5900_tests();
        omni::tests::run_binary_format_tests();
        omni::tests::run_option_tests();
        omni::tests::run_analysis_tests();
        std::cout << "All " << omni::identity::window_title << " tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
