set(SHADER_SOURCES
    "shaders/triangle.frag"
    "shaders/triangle.vert"
    "shaders/sprite_batch.frag"
    "shaders/sprite_batch.vert"
)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

foreach(SHADER IN LISTS SHADER_SOURCES)
    get_filename_component(FILENAME ${SHADER} NAME)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.spv" "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}"
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER}"
    )
    list(APPEND SHADERS "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILENAME}.spv")
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADERS})
