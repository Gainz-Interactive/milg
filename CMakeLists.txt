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
  GIT_PROGRESS TRUE)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 527b2c45af2f8964f95826bd16ab7c7ed372ae41
    GIT_PROGRESS TRUE)

FetchContent_Declare(
  VulkanUtilityLibraries
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Utility-Libraries.git
  GIT_TAG v1.3.285
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
  FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(sdl volk imgui VulkanUtilityLibraries)

set(TARGET_NAME milg)
set(
    SOURCE_FILES
    "src/main.cpp"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
target_compile_definitions(${TARGET_NAME} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_include_directories(${TARGET_NAME} PRIVATE ${imgui_SOURCE_DIR})
target_link_libraries(
    ${TARGET_NAME} PRIVATE
    SDL2::SDL2
    volk::volk
    Vulkan::UtilityHeaders
)
