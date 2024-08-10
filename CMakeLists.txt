cmake_minimum_required(VERSION 3.25)
project(milg)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(SDL_SHARED FALSE CACHE BOOL "Build SDL2 as a shared library")
set(SDL_STATIC TRUE CACHE BOOL "Build SDL2 as a static library")
set(SDL_TEST FALSE CACHE BOOL "Build the SDL2_test library")
set(SDL2_DISABLE_INSTALL TRUE CACHE BOOL "Disable installation of SDL2")
FetchContent_Declare(
  sdl
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.6
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  FIND_PACKAGE_ARGS NAMES SDL2
)

FetchContent_Declare(
  volk
  GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG 1.3.270
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 527b2c45af2f8964f95826bd16ab7c7ed372ae41
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

set(VMA_STATIC_VULKAN_FUNCTIONS FORCE BOOL OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS FORCE BOOL ON)
FetchContent_Declare(
  vma
  GIT_REPOSITORY
  https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.1.0
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG f75e8d1cad7d90d72ef7a4661f1b994ef78b4e31
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

FetchContent_Declare(
    voclib
    GIT_REPOSITORY https://github.com/blastbay/voclib.git
    GIT_TAG ffccd73b84e2a41b1f06d85ea703fd481ae8c9bb
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(sdl volk imgui spdlog miniaudio glm vma stb voclib)

find_package(Vulkan)
find_package(VulkanUtilityLibraries)

add_subdirectory(data)
add_subdirectory(src)
