add_library(
    libmilg
    INTERFACE
    "milg.hpp"
    "milg/asset.hpp"
    "milg/asset_store.hpp"
    "milg/error.hpp"
    "milg/event.hpp"
    "milg/layer.hpp"
    "milg/logging.hpp"
    "milg/map.hpp"
)
target_include_directories(
    libmilg
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    libmilg
    INTERFACE nlohmann_json::nlohmann_json
    INTERFACE glm::glm-header-only
)
