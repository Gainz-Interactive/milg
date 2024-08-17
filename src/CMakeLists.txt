add_subdirectory(audio)

set(TARGET_NAME milg)

set(
    GRAPHICS_SOURCE_FILES
    "graphics/buffer.cpp"
    "graphics/swapchain.cpp"
    "graphics/texture.cpp"
    "graphics/vk_context.cpp"
    "graphics/pipeline.cpp"
    "graphics/sprite_batch.cpp"
)

set(
    SOURCE_FILES
    "application.cpp"
    "asset.cpp"
    "asset_store.cpp"
    "imgui_layer.cpp"
    "layer.cpp"
    "logging.cpp"
    "main.cpp"
    "window.cpp"
    "${GRAPHICS_SOURCE_FILES}"
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
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(
    ${TARGET_NAME}
    SYSTEM
    PRIVATE ${imgui_SOURCE_DIR}
    PRIVATE ${vma_SOURCE_DIR}/include
    PRIVATE ${stb_SOURCE_DIR}
)
target_link_libraries(
    ${TARGET_NAME} PRIVATE
    SDL2::SDL2
    volk::volk
    Vulkan::UtilityHeaders
    spdlog::spdlog_header_only
    glm::glm-header-only
    nlohmann_json::nlohmann_json
    GPUOpen::VulkanMemoryAllocator
    libmilg
    milg_audio
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W3)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall)
endif()
