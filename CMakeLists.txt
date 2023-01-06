cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("Game of Life" VERSION 1.0)

add_subdirectory(./include/glad)

# external library
find_package(glfw3 3.3 REQUIRED)

add_executable(main main.cpp)

target_include_directories(main PUBLIC include)

target_link_libraries(main PUBLIC glfw)
target_link_libraries(main PUBLIC glad)

# copy resources to build directory
add_custom_command(TARGET main POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:main>/resources)
