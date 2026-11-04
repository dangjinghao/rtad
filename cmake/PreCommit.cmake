cmake_minimum_required(VERSION 3.14)

get_filename_component(RTAD_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(RTAD_BUILD_DIR "${RTAD_ROOT}/out/pre-commit")

function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Pre-commit command failed (${result}): ${ARGV}")
    endif()
endfunction()

run_checked("${CMAKE_COMMAND}" -S "${RTAD_ROOT}" -B "${RTAD_BUILD_DIR}"
    -DRTAD_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug)
run_checked("${CMAKE_COMMAND}" --build "${RTAD_BUILD_DIR}" --config Debug
    --parallel 2)

# Working directory also supports CTest versions before --test-dir was added.
execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure -C Debug
    WORKING_DIRECTORY "${RTAD_BUILD_DIR}"
    RESULT_VARIABLE result
)
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Pre-commit tests failed (${result})")
endif()
