#pragma once

#include <milg/graphics/sprite.hpp>
#include <milg/graphics/texture.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

namespace milg {
    typedef std::size_t Gid;

    class Tileset {
    public:
        Tileset() = delete;
        Tileset(const std::shared_ptr<graphics::Texture> &texture, const glm::ivec2 &tile_size, std::size_t columns,
                std::size_t margin, std::size_t spacing);
        Tileset(const Tileset &) = default;
        Tileset(Tileset &&)      = default;

        Tileset &operator=(const Tileset &) = default;
        Tileset &operator=(Tileset &&)      = default;

        ~Tileset() = default;

        const std::shared_ptr<graphics::Texture> &get_texture();

        std::size_t get_width();
        std::size_t get_height();

        const glm::ivec2 &get_tile_size();

        glm::vec4 get_uv(Gid gid);

    private:
        std::shared_ptr<graphics::Texture> texture;
        glm::ivec2                         tile_size;
        std::size_t                        columns;
        std::size_t                        margin;
        std::size_t                        spacing;
    };

    struct Tile {
        Gid gid;

        graphics::Sprite         sprite;
        std::shared_ptr<Tileset> tileset;
    };

    class Map {
    public:
        class Loader : public Asset::Loader {
        public:
            std::shared_ptr<void> load(std::ifstream &stream);
        };

        typedef std::size_t Id;

        class Layer {
        public:
            enum class Type {
                TILE,
                OBJECT,
            };

            Layer() = delete;
            Layer(const glm::vec2 &pos, const glm::ivec2 &size, const glm::ivec2 &tile_size,
                  std::vector<std::shared_ptr<Tile>> tiles);
            Layer(const Layer &) = delete;
            Layer(Layer &&)      = delete;

            Layer &operator=(const Layer &) = delete;
            Layer &operator=(Layer &&)      = delete;

            ~Layer() = default;

            std::shared_ptr<Tile> get_tile_at(const glm::vec2 &pos);

        private:
            glm::vec2  pos;
            glm::ivec2 size;
            glm::ivec2 tile_size;

            std::vector<std::shared_ptr<Tile>> tiles;
        };

        struct Object {
            const Id id;

            const std::string name;
            const std::string type;

            glm::vec2 pos;
            glm::vec2 size;

            std::map<std::string, nlohmann::json> properties;
        };

        Map() = delete;
        Map(const glm::ivec2 &size, const glm::ivec2 &tile_size, const std::vector<std::shared_ptr<Layer>> &tiles,
            const std::vector<std::shared_ptr<Object>> &objects);
        Map(const Map &) = default;
        Map(Map &&)      = default;

        Map &operator=(const Map &) = default;
        Map &operator=(Map &&)      = default;

        ~Map() = default;

        const glm::ivec2 &get_size();
        const glm::ivec2 &get_tile_size();

        std::shared_ptr<Tile>              get_tile(const std::string &layer, const glm::vec2 &pos);
        std::vector<std::shared_ptr<Tile>> get_tiles(const glm::vec2 &pos);

        std::shared_ptr<Object>              get_object(const char *name, const char *type);
        std::vector<std::shared_ptr<Object>> get_objects_at(const glm::vec2 &pos);

    private:
        glm::ivec2 size;
        glm::ivec2 tile_size;

        std::vector<std::shared_ptr<Layer>> tiles;

        std::vector<std::shared_ptr<Object>>              objects;
        std::multimap<std::string, std::weak_ptr<Object>> object_name_map;
        std::multimap<std::string, std::weak_ptr<Object>> object_type_map;
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Map::Layer::Type, {
                                                       {Map::Layer::Type::TILE, "tilelayer"},
                                                       {Map::Layer::Type::OBJECT, "objectgroup"},
                                                   })
} // namespace milg
