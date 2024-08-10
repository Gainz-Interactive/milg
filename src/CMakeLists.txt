set(TARGET_NAME milg)
set(
    SOURCE_FILES
    "application.cpp"
    "audio.cpp"
    "audio.hpp"
    "audio/node.hpp"
    "buffer.cpp"
    "imgui_layer.cpp"
    "layer.cpp"
    "logging.cpp"
    "main.cpp"
    "sprite_batch.cpp"
    "swapchain.cpp"
    "texture.cpp"
    "vk_context.cpp"
    "window.cpp"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

add_executable(${TARGET_NAME} ${SOURCE_FILES})
add_dependencies(${TARGET_NAME} shaders)
set_target_properties(
    ${TARGET_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_compile_definitions(${TARGET_NAME} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_include_directories(
    ${TARGET_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
    ${voclib_SOURCE_DIR}
    ${vma_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${glm_SOURCE_DIR}/glm
)
target_link_libraries(
    ${TARGET_NAME} PRIVATE
    SDL2::SDL2
    volk::volk
    Vulkan::UtilityHeaders
    spdlog::spdlog
    glm::glm
)
