cmake_minimum_required(VERSION 3.24)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

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
find_package(CLI11 CONFIG REQUIRED)

add_subdirectory(lib/sync-cpp) # emits sync-cpp link target

function(create_target NAME)
  add_executable(${NAME} source/main.cpp source/window.cpp
                         source/window_manager.cpp)
  target_include_directories(${NAME} PRIVATE source)
  target_link_libraries(
    ${NAME}
    PRIVATE glfw
            glad::glad
            glm::glm
            stb::stb
            siv::PerlinNoise
            spdlog::spdlog
            CLI11::CLI11
            sync-cpp)
endfunction()

# targets
create_target(main)

create_target(main-asan)
target_compile_options(main-asan PRIVATE -fsanitize=address,leak,undefined)
target_link_options(main-asan PRIVATE -fsanitize=address,leak,undefined)

create_target(main-tsan)
target_compile_options(main-tsan PRIVATE -fsanitize=thread)
target_link_options(main-tsan PRIVATE -fsanitize=thread)

# link resources to build directory
add_custom_command(
  TARGET main
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/resources
          $<TARGET_FILE_DIR:main>/resources)
