cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# compile options add_compile_options(-pg)                                    #
# enable profiling set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg") #
# enable pofiling

project("Game of Life" VERSION 2.0)

# external library
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(PerlinNoise CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_subdirectory(lib/sync-cpp) # emits sync-cpp link target

# main
add_executable(main source/main.cpp source/window.cpp source/window_manager.cpp)
target_include_directories(main PUBLIC source)
target_link_libraries(
  main
  PUBLIC glfw
         glad::glad
         glm::glm
         stb::stb
         siv::PerlinNoise
         spdlog::spdlog
         sync-cpp)

# target_compile_options(main PRIVATE -fsanitize=thread)
# target_link_options(main PRIVATE -fsanitize=thread)

# link resources to build directory
add_custom_command(
  TARGET main
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/resources
          $<TARGET_FILE_DIR:main>/resources)
