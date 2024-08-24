#include <milg/graphics/map.hpp>

#include <milg/core/asset_store.hpp>
#include <milg/core/logging.hpp>

#include <stb_image.h>

namespace milg {
    Tileset::Tileset(const nlohmann::json &json) {
        json["imagewidth"].get_to(this->width);
        json["imageheight"].get_to(this->height);

        json["tilewidth"].get_to(this->tile_width);
        json["tileheight"].get_to(this->tile_height);

        json["columns"].get_to(this->columns);

        json["spacing"].get_to(this->spacing);
        json["margin"].get_to(this->margin);

        json["image"].get_to(this->source);
    }

    const std::filesystem::path &Tileset::get_source() {
        return this->source;
    }

    std::size_t Tileset::get_width() {
        return this->width;
    }

    std::size_t Tileset::get_height() {
        return this->height;
    }

    std::size_t Tileset::get_tile_width() {
        return this->tile_width;
    }

    std::size_t Tileset::get_tile_height() {
        return this->tile_height;
    }

    glm::vec4 Tileset::get_uv(Gid gid) {
        gid = gid - 1;

        auto x = gid % this->columns;
        auto y = gid / this->columns;

        x = (x * this->tile_width) + this->margin + (x * this->spacing);
        y = (y * this->tile_height) + this->margin + (y * this->spacing);

        return {
            (float)x / (float)this->width,
            (float)y / (float)this->height,
            (float)(x + this->tile_width) / (float)this->width,
            (float)(y + this->tile_height) / (float)this->height,
        };
    }

    Map::Layer::Layer(Map *map, const nlohmann::json &json) {
        json["type"].get_to(this->type);

        json["data"].get_to(this->gids);

        json["x"].get_to(this->x);
        json["y"].get_to(this->y);
        json["width"].get_to(this->width);
        json["height"].get_to(this->height);

        this->map    = map;
    }

    std::vector<Tile> Map::Layer::get_tiles() {
        std::vector<Tile> ret;
        auto              map_width       = this->map->get_width();
        auto              map_tile_width  = this->map->get_tile_width();
        auto              map_tile_height = this->map->get_tile_height();

        for (Gid i = 0; i < this->height; i++) {
            for (Gid j = 0; j < this->width; j++) {
                auto gid     = this->gids[map_width * i + j];
                auto tileset = this->map->get_tileset_for_gid(gid);

                auto sprite = milg::graphics::Sprite{
                    .position = {j * map_tile_width, i * map_tile_height},
                    .size     = {map_tile_width, map_tile_height},
                    .uvs      = tileset->get_uv(gid),
                };

                ret.emplace_back(gid, sprite);
            }
        }

        return ret;
    }

    std::size_t Map::Layer::get_width() {
        return this->width;
    }

    std::size_t Map::Layer::get_height() {
        return this->height;
    }

    Map::Layer::Type Map::Layer::get_type() {
        return this->type;
    }

    Map::Map(const nlohmann::json &json) {
        json["width"].get_to(this->width);
        json["height"].get_to(this->height);

        json["tilewidth"].get_to(this->tile_width);
        json["tileheight"].get_to(this->tile_height);

        for (auto &tileset_json : json["tilesets"].items()) {
            auto &tileset   = tileset_json.value();
            auto  first_gid = tileset["firstgid"].get<std::size_t>();
            auto  source    = tileset["source"].get<std::string>();
            auto  asset     = asset_store::get_asset(source);

            tilesets.try_emplace(first_gid, std::make_unique<Tileset>(asset->get_preprocessed<nlohmann::json>()));
        }
        for (auto &layer : json["layers"]) {
            auto id = layer["id"].get<std::size_t>();

            layers[id] = std::make_unique<Layer>(this, layer);
        }
    }

    std::size_t Map::get_width() {
        return this->width;
    }

    std::size_t Map::get_height() {
        return this->height;
    }

    std::size_t Map::get_tile_width() {
        return this->tile_width;
    }

    std::size_t Map::get_tile_height() {
        return this->tile_height;
    }

    Map::Layer *Map::get_layer(std::size_t id) {
        if (auto iter = this->layers.find(id); iter != this->layers.end()) {
            return iter->second.get();
        }

        return nullptr;
    }

    Tileset *Map::get_tileset_for_gid(std::size_t gid) {
        Tileset *last_tileset = nullptr;

        for (auto &[k, v] : tilesets) {
            if (k == gid) {
                return v.get();
            }

            if (k > gid) {
                return last_tileset;
            }

            last_tileset = v.get();
        }

        return last_tileset;
    }
} // namespace milg
