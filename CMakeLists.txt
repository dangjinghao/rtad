cmake_minimum_required(VERSION 3.10)
project(rtad C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# --- Library ---
# Find all .c files in the src directory
file(GLOB RTAD_SOURCES "src/*.c")

# Create a library from the collected source files
add_library(rtad ${RTAD_SOURCES})

# Make the include directory public for the library
target_include_directories(rtad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


# --- Testing ---
# Enable testing with CTest
enable_testing()

# --- Integration Test (tests the library API) ---
add_executable(rtad_integration_test test/integration_test.c)
target_link_libraries(rtad_integration_test PRIVATE rtad)
add_test(NAME integration_test COMMAND rtad_integration_test)

# --- Unit Tests (tests all functions) ---
add_executable(rtad_unit_test test/unit_test.c)
target_link_libraries(rtad_unit_test PRIVATE rtad)
add_test(NAME unit_tests COMMAND rtad_unit_test)
