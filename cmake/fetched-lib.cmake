include(FetchContent)

FetchContent_Declare(
  sync-cpp
  GIT_REPOSITORY https://github.com/mrizaln/sync-cpp
  GIT_TAG 404d9267d8035fbdc6d58e1d493132ba6e908eae)
FetchContent_MakeAvailable(sync-cpp)

FetchContent_Declare(
  glfw-cpp
  GIT_REPOSITORY https://github.com/mrizaln/glfw-cpp
  GIT_TAG v0.3.0)
FetchContent_MakeAvailable(glfw-cpp)
