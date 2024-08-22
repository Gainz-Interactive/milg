set(TARGET_NAME milgame)

add_subdirectory(data)
add_executable(${TARGET_NAME} src/main.cpp)
target_compile_definitions(${TARGET_NAME} PRIVATE -DASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
