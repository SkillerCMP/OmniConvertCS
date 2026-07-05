cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/OmniIdentity.cmake")
project(OmniConvertCpp VERSION ${OMNI_VERSION} LANGUAGES CXX)

set(OMNI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(OMNI_GENERATED_INCLUDE_DIR "${OMNI_GENERATED_DIR}/include")
set(OMNI_GENERATED_RESOURCE_DIR "${OMNI_GENERATED_DIR}/win32")
file(MAKE_DIRECTORY "${OMNI_GENERATED_INCLUDE_DIR}/omni" "${OMNI_GENERATED_RESOURCE_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/identity.hpp.in"
    "${OMNI_GENERATED_INCLUDE_DIR}/omni/identity.hpp"
    @ONLY)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/OmniConvert.manifest.in"
    "${OMNI_GENERATED_RESOURCE_DIR}/OmniConvert.manifest"
    @ONLY)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/identity_version.rc.in"
    "${OMNI_GENERATED_RESOURCE_DIR}/identity_version.rc"
    @ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OMNI_BUILD_GUI "Build the native Win32 GUI on Windows" ON)
if(WIN32)
    set(_omni_cli_default OFF)
else()
    set(_omni_cli_default ON)
endif()
option(OMNI_BUILD_CLI "Build a separate command-line executable" ${_omni_cli_default})
option(OMNI_BUILD_TESTS "Build OmniConvert C++ tests" ON)
option(OMNI_REQUIRE_64_BIT "Reject 32-bit builds" ON)
option(OMNI_WINDOWS7_COMPAT "Target Windows 7 SP1 and avoid Windows 8+ APIs" ON)
option(OMNI_STATIC_MSVC_RUNTIME "Statically link the MSVC runtime" ${OMNI_WINDOWS7_COMPAT})

if(WIN32 AND OMNI_WINDOWS7_COMPAT)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "6.1")

    # Windows 7 desktop target. These definitions must be visible before any
    # Windows SDK or MSVC STL headers are included.
    add_compile_definitions(
        WINVER=0x0601
        _WIN32_WINNT=0x0601
        NTDDI_VERSION=0x06010000
    )

    # Windows 7 compatibility is supported with the VS2017-v141 through
    # VS2022-v143 compiler libraries. Newer toolsets may import APIs or runtime
    # components that are unavailable on Windows 7.
    if(MSVC AND DEFINED MSVC_TOOLSET_VERSION AND
       (MSVC_TOOLSET_VERSION LESS 141 OR MSVC_TOOLSET_VERSION GREATER 143))
        message(FATAL_ERROR
            "OMNI_WINDOWS7_COMPAT requires an MSVC v141, v142, or v143 toolset. "
            "Install the Desktop development with C++ workload and one of those optional toolsets.")
    endif()
endif()

if(MSVC AND OMNI_STATIC_MSVC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(OMNI_REQUIRE_64_BIT AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "OmniConvertCpp is configured for 64-bit builds only. Select an x64/arm64 toolchain.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(OmniWarnings)

add_library(omni_core STATIC
    src/core/cheat.cpp
    src/core/code_format.cpp
    src/core/crypt.cpp
    src/core/inline_crypt.cpp
    src/crypto/arcfour.cpp
    src/convert/conversion_service.cpp
    src/analysis/raw_plausibility.cpp
    src/analysis/e001_fixer.cpp
    src/translate/translation_error.cpp
    src/translate/transpose_mode.cpp
    src/translate/cb_gs_semantics.cpp
    src/translate/armax_conditions.cpp
    src/translate/translation_helpers.cpp
    src/translate/translator.cpp
    src/translate/original_translation_helpers.cpp
    src/translate/original_standard_to_standard.cpp
    src/translate/original_standard_to_armax.cpp
    src/translate/original_armax_to_standard.cpp
    src/translate/standard_to_standard.cpp
    src/translate/standard_to_armax.cpp
    src/translate/armax_to_standard.cpp
    src/devices/action_replay/ar_crypto.cpp
    src/devices/action_replay/ar_batch.cpp
    src/devices/armax/armax_codec.cpp
    src/devices/armax/armax_crypto.cpp
    src/devices/armax/armax_disc_hash.cpp
    src/devices/armax/armax_options.cpp
    src/devices/armax/armax_tables.cpp
    src/devices/codebreaker/cb1_crypto.cpp
    src/devices/codebreaker/cb7_math.cpp
    src/devices/codebreaker/cb7_tables.cpp
    src/devices/codebreaker/cb7_crypto.cpp
    src/devices/codebreaker/cb_batch.cpp
    src/devices/codebreaker/cb_file_crypto.cpp
    src/devices/gameshark/gs3_crypto.cpp
    src/devices/gameshark/gs3_tables.cpp
    src/devices/gameshark/gs3_verifier.cpp
    src/devices/gameshark/gs3_file.cpp
    src/formats/binary/application_formats.cpp
    src/formats/binary/binary_common.cpp
    src/formats/binary/armax_bin.cpp
    src/formats/binary/cbc.cpp
    src/formats/binary/p2m.cpp
    src/formats/binary/swap_magic_bin.cpp
    src/formats/mips/mips_r5900_text.cpp
    src/formats/pnach/pnach_parser.cpp
    src/formats/pnach/pnach_writer.cpp
    src/formats/pnach/pnach_crc.cpp
    src/formats/text/copy_paste_cleaner.cpp
    src/formats/text/text_cheat_parser.cpp
    src/formats/text/text_cheat_writer.cpp
    src/util/hex.cpp
    src/util/newlines.cpp
)

target_include_directories(omni_core PUBLIC src "${OMNI_GENERATED_INCLUDE_DIR}")
target_compile_features(omni_core PUBLIC cxx_std_17)
omni_enable_warnings(omni_core)

set(_omni_need_cli_support ${OMNI_BUILD_CLI})
if(WIN32 AND OMNI_BUILD_GUI)
    set(_omni_need_cli_support ON)
endif()

if(_omni_need_cli_support)
    add_library(omni_cli_support STATIC
        src/cli/cli_app.cpp
        src/cli/analyze_cli.cpp
        src/cli/cli_app.hpp
        src/cli/analyze_cli.hpp
    )
    target_link_libraries(omni_cli_support PUBLIC omni_core)
    target_include_directories(omni_cli_support PUBLIC src)
    omni_enable_warnings(omni_cli_support)
endif()

if(OMNI_BUILD_CLI)
    add_executable(omniconvert_cli src/cli/main.cpp)
    target_link_libraries(omniconvert_cli PRIVATE omni_cli_support)
    omni_enable_warnings(omniconvert_cli)
    if(NOT WIN32 OR NOT OMNI_BUILD_GUI)
        set_target_properties(omniconvert_cli PROPERTIES OUTPUT_NAME "${OMNI_EXECUTABLE_NAME}")
    endif()
    if(WIN32 AND OMNI_WINDOWS7_COMPAT)
        if(MSVC)
            target_link_options(omniconvert_cli PRIVATE "/SUBSYSTEM:CONSOLE,6.01")
        elseif(MINGW)
            target_link_options(omniconvert_cli PRIVATE
                "-Wl,--major-subsystem-version,6"
                "-Wl,--minor-subsystem-version,1")
        endif()
    endif()
endif()

if(OMNI_BUILD_GUI)
    if(WIN32)
        enable_language(RC)
        add_executable(omniconvert_gui WIN32
            src/gui/win32/main.cpp
            src/gui/win32/app_state.cpp
            src/gui/win32/armax_options_dialog.cpp
            src/gui/win32/armax_disc_hash_dialog.cpp
            src/gui/win32/command_handlers.cpp
            src/gui/win32/context_controls.cpp
            src/gui/win32/conversion_controller.cpp
            src/gui/win32/dialog_proc.cpp
            src/gui/win32/edit_commands.cpp
            src/gui/win32/file_commands.cpp
            src/gui/win32/format_menu.cpp
            src/gui/win32/gs3_key_menu.cpp
            src/gui/win32/hex_edit.cpp
            src/gui/win32/key_dialog.cpp
            src/gui/win32/pnach_crc_dialog.cpp
            src/gui/win32/options_menu.cpp
            src/gui/win32/settings.cpp
            src/gui/win32/layout.cpp
            src/gui/win32/text_control.cpp
            src/gui/win32/transpose_mode_menu.cpp
            src/gui/win32/resources/resource.rc
            "${OMNI_GENERATED_RESOURCE_DIR}/identity_version.rc"
            src/gui/win32/app_state.hpp
            src/gui/win32/armax_options_dialog.hpp
            src/gui/win32/armax_disc_hash_dialog.hpp
            src/gui/win32/command_handlers.hpp
            src/gui/win32/context_controls.hpp
            src/gui/win32/conversion_controller.hpp
            src/gui/win32/dialog_proc.hpp
            src/gui/win32/edit_commands.hpp
            src/gui/win32/file_commands.hpp
            src/gui/win32/format_menu.hpp
            src/gui/win32/gs3_key_menu.hpp
            src/gui/win32/hex_edit.hpp
            src/gui/win32/key_dialog.hpp
            src/gui/win32/pnach_crc_dialog.hpp
            src/gui/win32/options_menu.hpp
            src/gui/win32/settings.hpp
            src/gui/win32/layout.hpp
            src/gui/win32/text_control.hpp
            src/gui/win32/transpose_mode_menu.hpp
            src/gui/win32/resources/resource.h
            src/gui/win32/resources/oc.ico
        )
        target_include_directories(omniconvert_gui PRIVATE
            src
            src/gui/win32/resources
            "${OMNI_GENERATED_RESOURCE_DIR}"
        )
        target_compile_definitions(omniconvert_gui PRIVATE
            UNICODE
            _UNICODE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
        target_link_libraries(omniconvert_gui PRIVATE
            omni_core
            omni_cli_support
            comctl32
            comdlg32
            shell32
            user32
            gdi32
        )
        if(MSVC)
            target_link_options(omniconvert_gui PRIVATE
                "/MANIFEST:EMBED"
                "/MANIFESTINPUT:${OMNI_GENERATED_RESOURCE_DIR}/OmniConvert.manifest")
        elseif(MINGW)
            target_compile_definitions(omniconvert_gui PRIVATE
                $<$<COMPILE_LANGUAGE:RC>:OMNI_EMBED_MANIFEST_RESOURCE>)
        endif()
        omni_enable_warnings(omniconvert_gui)
        set_target_properties(omniconvert_gui PROPERTIES OUTPUT_NAME "${OMNI_EXECUTABLE_NAME}")
        if(OMNI_WINDOWS7_COMPAT)
            if(MSVC)
                target_link_options(omniconvert_gui PRIVATE "/SUBSYSTEM:WINDOWS,6.01")
            elseif(MINGW)
                target_link_options(omniconvert_gui PRIVATE
                    "-Wl,--major-subsystem-version,6"
                    "-Wl,--minor-subsystem-version,1")
            endif()
        endif()
    else()
        message(STATUS "Native Win32 GUI skipped on this host. The core, CLI, and tests remain portable.")
    endif()
endif()

if(OMNI_BUILD_TESTS)
    enable_testing()
    add_executable(omni_tests
        src/tests/test_main.cpp
        src/tests/test_core.cpp
        src/tests/test_copy_paste_cleanup.cpp
        src/tests/test_cmp_output_mode.cpp
        src/tests/test_format_aliases.cpp
        src/tests/test_action_replay.cpp
        src/tests/test_codebreaker.cpp
        src/tests/test_gameshark.cpp
        src/tests/test_armax.cpp
        src/tests/test_armax_translation.cpp
        src/tests/test_vector_inventory.cpp
        src/tests/test_translation.cpp
        src/tests/test_cb_gs_translation.cpp
        src/tests/test_transpose_modes.cpp
        src/tests/test_pnach.cpp
        src/tests/test_pnach_crc.cpp
        src/tests/test_inline_mode.cpp
        src/tests/test_mips_r5900.cpp
        src/tests/test_binary_formats.cpp
        src/tests/test_options.cpp
        src/tests/test_armax_disc_hash.cpp
        src/tests/test_analysis.cpp
        src/tests/crypto/vector_file.cpp
    )
    target_link_libraries(omni_tests PRIVATE omni_core)
    target_compile_definitions(omni_tests PRIVATE
        OMNI_TEST_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/tests/vectors/crypto"
    )
    omni_enable_warnings(omni_tests)
    add_test(NAME omni_tests COMMAND omni_tests)
    add_test(NAME win32_gui_parity_static
        COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/ValidateWin32Parity.cmake)
    if(TARGET omniconvert_cli)
        set(_omni_cli_test_target omniconvert_cli)
    elseif(TARGET omniconvert_gui)
        set(_omni_cli_test_target omniconvert_gui)
    endif()
    if(DEFINED _omni_cli_test_target)
        add_test(NAME cli_analyze_workflow
            COMMAND ${CMAKE_COMMAND}
                -DCLI=$<TARGET_FILE:${_omni_cli_test_target}>
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-analysis-test
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/TestAnalyzeCli.cmake)
        add_test(NAME cli_armax_disc_hash_workflow
            COMMAND ${CMAKE_COMMAND}
                -DCLI=$<TARGET_FILE:${_omni_cli_test_target}>
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-armax-disc-hash-test
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/TestArmaxDiscHashCli.cmake)
    endif()
endif()

include(GNUInstallDirs)
if(TARGET omniconvert_gui)
    install(TARGETS omniconvert_gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET omniconvert_cli)
    install(TARGETS omniconvert_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
