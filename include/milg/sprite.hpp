#pragma once

#include <glm/glm.hpp>

namespace milg::graphics {
    struct Sprite {
        glm::vec2 position      = {0.0f, 0.0f};
        glm::vec2 size          = {0.0f, 0.0f};
        glm::vec4 uvs           = {0.0f, 0.0f, 1.0f, 1.0f};
        glm::vec4 color         = {1.0f, 1.0f, 1.0f, 1.0f};
        float     rotation      = 0.0f;
        float     texture_index = 0.0f;

        static constexpr uint32_t ATTRIB_COUNT = 14;
    };

    static_assert(sizeof(Sprite) == Sprite::ATTRIB_COUNT * sizeof(float),
                  "Sprite struct might not be packed correctly");
} // namespace milg::graphics
