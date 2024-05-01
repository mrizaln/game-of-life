include(FetchContent)

FetchContent_Declare(
  sync-cpp
  GIT_REPOSITORY https://github.com/mrizaln/sync-cpp
  GIT_TAG 404d9267d8035fbdc6d58e1d493132ba6e908eae)
FetchContent_MakeAvailable(sync-cpp)

FetchContent_Declare(
  glfw-cpp
  GIT_REPOSITORY https://github.com/mrizaln/glfw-cpp
  GIT_TAG 92a906f4ba017140f0e3929396677cb33ae7ccc5)
FetchContent_MakeAvailable(glfw-cpp)
