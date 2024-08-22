set(TARGET_NAME milgame)

add_subdirectory(data)
add_executable(${TARGET_NAME} src/main.cpp)
