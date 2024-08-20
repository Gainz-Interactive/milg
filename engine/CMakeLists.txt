set(TARGET_NAME milg-engine)
set(
    SOURCE_FILES
    "src/core/application.cpp"
    "src/core/asset.cpp"
    "src/core/asset_store.cpp"
    "src/core/imgui_layer.cpp"
    "src/core/layer.cpp"
    "src/core/logging.cpp"
    "src/core/window.cpp"

    "src/audio/engine.cpp"
    "include/milg/audio/engine.hpp"
    "include/milg/audio/node.hpp"
    "src/audio/sound.cpp"
    "include/milg/audio/sound.hpp"
    "src/audio/vocoder_node.cpp"
    "include/milg/audio/vocoder_node.hpp"

    "src/graphics/buffer.cpp"
    "src/graphics/swapchain.cpp"
    "src/graphics/texture.cpp"
    "src/graphics/vk_context.cpp"
    "src/graphics/pipeline.cpp"
    "src/graphics/sprite_batch.cpp"
    "src/graphics/map.cpp"

    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

add_library(${TARGET_NAME} ${SOURCE_FILES})
set_target_properties(
    ${TARGET_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
target_compile_definitions(${TARGET_NAME} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(
    ${TARGET_NAME}
    SYSTEM
    PUBLIC ${imgui_SOURCE_DIR}
    PRIVATE ${vma_SOURCE_DIR}/include
    PRIVATE ${stb_SOURCE_DIR}
    PRIVATE ${voclib_SOURCE_DIR}
    PUBLIC ${miniaudio_SOURCE_DIR}
)
target_link_libraries(
    ${TARGET_NAME} PUBLIC
    SDL2::SDL2
    volk::volk
    Vulkan::UtilityHeaders
    spdlog::spdlog_header_only
    glm::glm-header-only
    nlohmann_json::nlohmann_json
    GPUOpen::VulkanMemoryAllocator
)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W3)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall)
endif()
