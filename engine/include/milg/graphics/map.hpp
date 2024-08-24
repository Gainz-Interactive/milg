#pragma once

#include <milg/graphics/sprite.hpp>

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
        Tileset(const nlohmann::json &json);
        Tileset(const Tileset &) = default;
        Tileset(Tileset &&)      = default;

        Tileset &operator=(const Tileset &) = default;
        Tileset &operator=(Tileset &&)      = default;

        ~Tileset() = default;

        const std::filesystem::path &get_source();

        const std::string &get_name();

        std::size_t get_tile_count();

        std::size_t get_width();
        std::size_t get_height();

        std::size_t get_tile_width();
        std::size_t get_tile_height();

        glm::vec4 get_uv(Gid gid);

    private:
        std::string name;

        std::size_t tile_count;

        std::size_t width;
        std::size_t height;

        std::size_t tile_width;
        std::size_t tile_height;

        std::size_t columns;

        std::size_t spacing;
        std::size_t margin;

        std::filesystem::path source;
    };

    struct Tile {
        Gid gid;

        graphics::Sprite         sprite;
        std::shared_ptr<Tileset> tileset;
    };

    class Map {
    public:
        typedef std::size_t Id;

        enum class LayerType {
            TILE,
            OBJECT,
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
        Map(const nlohmann::json &json);
        Map(const Map &) = default;
        Map(Map &&)      = default;

        Map &operator=(const Map &) = default;
        Map &operator=(Map &&)      = default;

        ~Map() = default;

        std::size_t get_width();
        std::size_t get_height();

        std::size_t get_tile_width();
        std::size_t get_tile_height();

        std::vector<Tile>                    *get_layer(const std::string &name);
        std::shared_ptr<Object>               get_object(const char *name, const char *type);
        std::vector<std::shared_ptr<Tileset>> get_tilesets();

    private:
        void process_tile_layer(std::map<Gid, std::weak_ptr<Tileset>>, const nlohmann::json &json);
        void process_object_layer(const nlohmann::json &json);

        std::size_t width;
        std::size_t height;

        std::size_t tile_width;
        std::size_t tile_height;

        std::vector<std::shared_ptr<Tileset>> tilesets;

        std::map<std::string, std::vector<Tile>> layers;

        std::vector<std::shared_ptr<Object>>              objects;
        std::multimap<std::string, std::weak_ptr<Object>> object_name_map;
        std::multimap<std::string, std::weak_ptr<Object>> object_type_map;
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Map::LayerType, {
                                                     {Map::LayerType::TILE, "tilelayer"},
                                                     {Map::LayerType::OBJECT, "objectgroup"},
                                                 })
} // namespace milg
