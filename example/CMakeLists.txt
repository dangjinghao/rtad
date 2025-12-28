cmake_minimum_required(VERSION 3.14)
project(rtad_example C)

# Add the rtad library from parent directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/rtad)

# Create the example executable
add_executable(rtad_example example.c)

# Link to rtad library
target_link_libraries(rtad_example PRIVATE rtad)
