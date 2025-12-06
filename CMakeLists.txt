cmake_minimum_required(VERSION 3.14)
project(rtad C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the main library
add_library(rtad src/rtad.c)
target_include_directories(rtad PUBLIC include)


set(BUILD_SHARED_LIBS OFF)
set(WITH_TESTS OFF)

include(FetchContent)
FetchContent_Declare(
  cmocka
  GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
  GIT_TAG        cmocka-2.0.0
)
FetchContent_MakeAvailable(cmocka)

enable_testing()

# Unit tests
add_executable(rtad_unit_test test/unit_test.c)

target_include_directories(rtad_unit_test PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(rtad_unit_test PRIVATE rtad cmocka)
add_test(NAME unit_tests COMMAND rtad_unit_test)

# Integration tests
add_executable(rtad_integration_test test/integration_test.c)

target_include_directories(rtad_integration_test PRIVATE ${cmocka_SOURCE_DIR}/include)
target_link_libraries(rtad_integration_test PRIVATE rtad cmocka)
add_test(NAME integration_tests COMMAND rtad_integration_test)
