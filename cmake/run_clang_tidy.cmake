if (NOT DEFINED CLANG_TIDY_EXECUTABLE)
    set(CLANG_TIDY_EXECUTABLE clang-tidy)
endif ()

if (NOT DEFINED COMPILE_COMMANDS_FILE)
    message(FATAL_ERROR "COMPILE_COMMANDS_FILE is required")
endif ()

if (NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif ()

if (NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif ()

if (NOT EXISTS "${COMPILE_COMMANDS_FILE}")
    message(FATAL_ERROR
            "Compilation database not found: ${COMPILE_COMMANDS_FILE}\n"
            "Configure with: cmake --preset ninja-lint")
endif ()

file(TO_CMAKE_PATH "${SOURCE_ROOT}" source_root)
if (NOT source_root MATCHES "/$")
    string(APPEND source_root "/")
endif ()

file(READ "${COMPILE_COMMANDS_FILE}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
if (command_count EQUAL 0)
    message(FATAL_ERROR "No entries found in ${COMPILE_COMMANDS_FILE}")
endif ()

math(EXPR last_command_index "${command_count} - 1")
foreach (command_index RANGE 0 ${last_command_index})
    string(JSON source_file GET "${compile_commands_json}" ${command_index} file)
    file(TO_CMAKE_PATH "${source_file}" normalized_source_file)
    string(FIND "${normalized_source_file}" "${source_root}" source_root_position)

    if (source_root_position EQUAL 0 AND normalized_source_file MATCHES "\\.cpp$")
        list(APPEND source_files "${source_file}")
    endif ()
endforeach ()

list(LENGTH source_files source_file_count)
if (source_file_count EQUAL 0)
    message(FATAL_ERROR
            "No project source files found in ${COMPILE_COMMANDS_FILE}")
endif ()

message(STATUS "Running clang-tidy on ${source_file_count} project source files")
foreach (source_file IN LISTS source_files)
    file(RELATIVE_PATH relative_source_file "${SOURCE_ROOT}" "${source_file}")
    message(STATUS "  ${relative_source_file}")
endforeach ()

if (NOT DEFINED CLANG_TIDY_JOBS OR CLANG_TIDY_JOBS STREQUAL "")
    set(CLANG_TIDY_JOBS 8)
endif ()

if (RUN_CLANG_TIDY_EXECUTABLE AND Python3_EXECUTABLE)
    set(source_filter "${source_root}")
    string(REPLACE "/" "[/\\\\]" source_filter "${source_filter}")
    set(source_filter "^${source_filter}.*\\.cpp$")

    message(STATUS "Using run-clang-tidy with ${CLANG_TIDY_JOBS} jobs")
    message(STATUS "Source filter: ${source_filter}")

    execute_process(
            COMMAND "${Python3_EXECUTABLE}"
                    "${RUN_CLANG_TIDY_EXECUTABLE}"
                    -clang-tidy-binary "${CLANG_TIDY_EXECUTABLE}"
                    -p "${BUILD_DIR}"
                    -source-filter "${source_filter}"
                    -extra-arg=-Wno-unused-command-line-argument
                    -j "${CLANG_TIDY_JOBS}"
                    -quiet
            RESULT_VARIABLE run_clang_tidy_result
            COMMAND_ECHO STDOUT
    )

    if (NOT run_clang_tidy_result EQUAL 0)
        message(FATAL_ERROR "clang-tidy failed")
    endif ()
else ()
    message(STATUS "run-clang-tidy or Python was not found. Falling back to sequential clang-tidy")

    foreach (source_file IN LISTS source_files)
        file(RELATIVE_PATH relative_source_file "${SOURCE_ROOT}" "${source_file}")
        message(STATUS "clang-tidy ${relative_source_file}")

        execute_process(
                COMMAND "${CLANG_TIDY_EXECUTABLE}" --quiet -p "${BUILD_DIR}" -extra-arg=-Wno-unused-command-line-argument "${source_file}"
                RESULT_VARIABLE clang_tidy_result
                COMMAND_ECHO STDOUT
        )

        if (NOT clang_tidy_result EQUAL 0)
            list(APPEND failed_files "${relative_source_file}")
        endif ()
    endforeach ()

    if (failed_files)
        list(JOIN failed_files "\n  " failed_file_list)
        message(FATAL_ERROR
                "clang-tidy failed for:\n  ${failed_file_list}")
    endif ()
endif ()
