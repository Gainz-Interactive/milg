add_library(
    milg_audio
    OBJECT
    "engine.cpp"
    "engine.hpp"
    "layer.cpp"
    "layer.hpp"
    "node.hpp"
    "sound.cpp"
    "sound.hpp"
    "vocoder_node.cpp"
    "vocoder_node.hpp"
)
target_include_directories(
    milg_audio
    PRIVATE ${imgui_SOURCE_DIR}
    PRIVATE ${voclib_SOURCE_DIR}
    PUBLIC ${miniaudio_SOURCE_DIR}
)
target_link_libraries(
    milg_audio
    PUBLIC libmilg
    PUBLIC SDL2::SDL2
    PUBLIC spdlog::spdlog
)
