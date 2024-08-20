#pragma once

#include <milg/graphics/sprite.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

namespace milg {
    typedef std::size_t Gid;

    struct Tile {
        Gid gid;

        graphics::Sprite sprite;
    };

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

        std::size_t get_width();
        std::size_t get_height();

        std::size_t get_tile_width();
        std::size_t get_tile_height();

        glm::vec4 get_uv(Gid gid);

    private:
        std::size_t width;
        std::size_t height;

        std::size_t tile_width;
        std::size_t tile_height;

        std::size_t columns;

        std::size_t spacing;
        std::size_t margin;

        std::filesystem::path source;
    };

    class Map {
    public:
        class Layer {
        public:
            enum class Type {
                TILE,
                OBJECT,
            };

            Layer() = delete;
            Layer(Map *map, const nlohmann::json &json);
            Layer(const Layer &) = default;
            Layer(Layer &&)      = default;

            Layer &operator=(const Layer &) = default;
            Layer &operator=(Layer &&)      = default;

            ~Layer() = default;

            std::vector<Tile> get_tiles();

            std::size_t get_width();
            std::size_t get_height();

            Type get_type();

        private:
            Type type;

            std::vector<Gid> gids;

            std::size_t x;
            std::size_t y;
            std::size_t width;
            std::size_t height;

            Map *map;
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

        Layer               *get_layer(std::size_t id);
        std::vector<Layer &> get_layers();

        Tileset *get_tileset_for_gid(Gid gid);

    private:
        std::size_t width;
        std::size_t height;

        std::size_t tile_width;
        std::size_t tile_height;

        std::map<std::size_t, std::unique_ptr<Tileset>> tilesets;
        std::map<std::size_t, std::unique_ptr<Layer>>   layers;
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Map::Layer::Type, {
                                                       {Map::Layer::Type::TILE, "tilelayer"},
                                                       {Map::Layer::Type::OBJECT, "objectgroup"},
                                                   })
} // namespace milg
