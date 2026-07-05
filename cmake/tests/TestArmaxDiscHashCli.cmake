if(NOT DEFINED CLI)
    message(FATAL_ERROR "CLI was not supplied")
endif()
if(NOT DEFINED WORK)
    message(FATAL_ERROR "WORK was not supplied")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# Keep the synthetic CNF on one line. CMake opens file(WRITE) in text mode on
# Windows, so embedding explicit \r\n would become \r\r\n and change the
# byte-for-byte AR MAX disc hash. BOOT2 alone is sufficient for this test.
file(WRITE "${WORK}/SYSTEM(1).CNF"
    "BOOT2 = cdrom0:\\SLUS_204.97;1")
file(WRITE "${WORK}/SLUS_204.97" "Synthetic PS2 ELF prefix for AR MAX disc-hash CLI regression.")

execute_process(
    COMMAND "${CLI}" armax-disc-hash "${WORK}/SYSTEM(1).CNF"
            --game "Synthetic Game" --mapping "${WORK}/ArmaxDiscHash.json"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "armax-disc-hash CLI failed (${result}):\n${error}")
endif()

string(FIND "${output}" "BOOT2: SLUS_204.97" boot_found)
if(boot_found EQUAL -1)
    message(FATAL_ERROR "armax-disc-hash CLI did not parse BOOT2:\n${output}")
endif()
# The command was intentionally invoked without --elf. A zero exit status,
# the expected BOOT2 basename in the resolved ELF line, and a saved mapping
# prove that same-root auto-resolution succeeded. Avoid comparing absolute
# path spellings because Windows/CMake may differ in separators, drive-letter
# case, short/long path form, or runner canonicalization.
string(REPLACE "\\" "/" output_normalized "${output}")
string(REGEX MATCH "ELF:[ \t]*[^\r\n]*SLUS_204[.]97" elf_line "${output_normalized}")
if(elf_line STREQUAL "")
    message(FATAL_ERROR
        "armax-disc-hash CLI did not report the auto-resolved root ELF.\n"
        "Expected ELF basename: SLUS_204.97\n"
        "Output:\n${output}")
endif()
string(FIND "${output}" "DiscHash: E39512B1" hash_found)
if(hash_found EQUAL -1)
    message(FATAL_ERROR
        "armax-disc-hash CLI produced the wrong synthetic hash.\n"
        "Expected: DiscHash: E39512B1\n"
        "Output:\n${output}")
endif()

file(READ "${WORK}/ArmaxDiscHash.json" mapping)
string(FIND "${mapping}" "SLUS_204.97>" mapping_elf)
string(FIND "${mapping}" ">Synthetic Game" mapping_name)
if(mapping_elf EQUAL -1 OR mapping_name EQUAL -1)
    message(FATAL_ERROR "ArmaxDiscHash.json was not saved correctly:\n${mapping}")
endif()
