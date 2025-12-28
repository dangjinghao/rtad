cmake_minimum_required(VERSION 3.14)
project(rtad VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add the main library
add_library(rtad src/rtad.c)
target_include_directories(rtad PUBLIC include)

# Set library properties
set_target_properties(rtad PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER include/rtad.h
)

# Option: whether to build tests
option(RTAD_BUILD_TESTS "Build tests" OFF)

if(RTAD_BUILD_TESTS)
    # Add a version of the library for testing, with private functions exposed
    add_library(rtad_test OBJECT src/rtad.c)
    target_include_directories(rtad_test PRIVATE include)
    target_compile_definitions(rtad_test PRIVATE RTAD_TEST)

    # Disable shared libs and tests for cmocka
    # On Windows, running tests with cmocka dll causes issues
    set(BUILD_SHARED_LIBS OFF)
    # Disable cmocka tests to avoid conflicts
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
    target_include_directories(rtad_unit_test PRIVATE src)
    # add test macro RTAD_TEST
    target_compile_definitions(rtad_unit_test PRIVATE RTAD_TEST)

    target_link_libraries(rtad_unit_test PRIVATE rtad_test cmocka)
    add_test(NAME unit_tests COMMAND rtad_unit_test)

    # Integration tests
    add_executable(rtad_integration_test test/integration_test.c)

    target_include_directories(rtad_integration_test PRIVATE ${cmocka_SOURCE_DIR}/include)
    target_include_directories(rtad_integration_test PRIVATE include)
    # add test macro RTAD_TEST

    target_link_libraries(rtad_integration_test PRIVATE rtad_test cmocka)
    add_test(NAME integration_tests COMMAND rtad_integration_test)
endif()
