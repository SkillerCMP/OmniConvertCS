if(NOT DEFINED CLI OR NOT EXISTS "${CLI}")
    message(FATAL_ERROR "CLI executable was not provided: ${CLI}")
endif()
if(NOT DEFINED WORK)
    set(WORK "${CMAKE_CURRENT_BINARY_DIR}/cli-analysis-test")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/input/sub" "${WORK}/output")

file(WRITE "${WORK}/input/sub/mixed.txt"
"+Wrong AR2 Label , CRYPT_AR1\n"
"0E3C7DF2 1853E59E\n"
"EE8AA78A BCBDF29A\n"
"+Normal Raw , CRYPT_RAW\n"
"20100000 DEADBEEF\n")
execute_process(
    COMMAND "${CLI}" -af -o "${WORK}/output" RAW "${WORK}/input"
    RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "AnalyzeFix failed (${rc})\n${stdout}\n${stderr}")
endif()
file(READ "${WORK}/output/CRYPT_RAW/sub/mixed.txt" fixed_output)
if(NOT fixed_output MATCHES "F0145714 00145717")
    message(FATAL_ERROR "AnalyzeFix did not recover the mislabeled AR2 code:\n${fixed_output}")
endif()
file(READ "${WORK}/output/CRYPT_RAW/Analysis/sub/mixed.analysis.txt" fixed_report)
if(NOT fixed_report MATCHES "FIXED:.*CRYPT_AR1.*CRYPT_AR2")
    message(FATAL_ERROR "AnalyzeFix report did not record the corrected crypt:\n${fixed_report}")
endif()

file(WRITE "${WORK}/bad.txt"
"+Bad Raw , CRYPT_RAW\n"
"B0100000 00000001\n"
"+Good Raw , CRYPT_RAW\n"
"20100000 DEADBEEF\n")
execute_process(
    COMMAND "${CLI}" -a -o "${WORK}/analyze" CRAW "${WORK}/bad.txt"
    RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT rc EQUAL 2)
    message(FATAL_ERROR "Analyze was expected to return 2, returned ${rc}\n${stdout}\n${stderr}")
endif()
file(READ "${WORK}/analyze/CRYPT_CRAW/bad.txt" analyzed_output)
if(NOT analyzed_output MATCHES "B0100000 00000001" OR
   NOT analyzed_output MATCHES "20100000 DEADBEEF")
    message(FATAL_ERROR "Analyze did not preserve failed and later valid rows:\n${analyzed_output}")
endif()
file(READ "${WORK}/analyze/CRYPT_CRAW/Analysis/bad.analysis.txt" analyzed_report)
if(NOT analyzed_report MATCHES "TypeBNotAllowed")
    message(FATAL_ERROR "Analyze report did not include the plausibility rule:\n${analyzed_report}")
endif()

file(WRITE "${WORK}/e001.txt"
"+Condition , CRYPT_CRAW\n"
"E0011234 001ABCDE\n")
execute_process(
    COMMAND "${CLI}" -e -o "${WORK}/e001out" CRAW "${WORK}/e001.txt"
    RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "E001 rewrite workflow failed (${rc})\n${stdout}\n${stderr}")
endif()
file(READ "${WORK}/e001out/CRYPT_CRAW/e001.txt" e001_output)
if(NOT e001_output MATCHES "D01ABCDE 00001234")
    message(FATAL_ERROR "E001 rewrite output is incorrect:\n${e001_output}")
endif()
