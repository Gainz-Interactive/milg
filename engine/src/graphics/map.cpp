#include <milg/graphics/map.hpp>

#include <milg/core/asset.hpp>
#include <milg/core/logging.hpp>

#include <stb_image.h>

namespace milg {
    Tileset::Tileset(const std::shared_ptr<graphics::Texture> &texture, const glm::ivec2 &tile_size,
                     std::size_t columns, std::size_t margin, std::size_t spacing)
        : texture(texture), tile_size(tile_size), columns(columns), margin(margin), spacing(spacing) {
    }

    const std::shared_ptr<graphics::Texture> &Tileset::get_texture() {
        return this->texture;
    }

    std::size_t Tileset::get_width() {
        return this->texture->width();
    }

    std::size_t Tileset::get_height() {
        return this->texture->height();
    }

    const glm::ivec2 &Tileset::get_tile_size() {
        return this->tile_size;
    }

    glm::vec4 Tileset::get_uv(Gid gid) {
        gid = gid - 1;

        auto x = gid % this->columns;
        auto y = gid / this->columns;

        x = (x * this->tile_size.x) + this->margin + (x * this->spacing);
        y = (y * this->tile_size.y) + this->margin + (y * this->spacing);

        auto width  = this->get_width();
        auto height = this->get_height();

        return {
            (float)x / (float)width,
            (float)y / (float)height,
            (float)(x + this->tile_size.x) / (float)width,
            (float)(y + this->tile_size.y) / (float)height,
        };
    }

    Map::Layer::Layer(const glm::vec2 &pos, const glm::ivec2 &size, const glm::ivec2 &tile_size,
                      std::vector<std::shared_ptr<Tile>> tiles)
        : pos(pos), size(size), tile_size(tile_size), tiles(tiles) {
    }

    std::shared_ptr<Tile> Map::Layer::get_tile_at(const glm::vec2 &pos) {
        if (pos.x < this->pos.x || pos.y < this->pos.y) {
            return nullptr;
        }

        auto layer_end = this->pos + glm::vec2{this->size.x * this->tile_size.x, this->size.y * this->tile_size.y};

        if (pos.x >= layer_end.x || pos.y >= layer_end.y) {
            return nullptr;
        }

        auto grid_pos = static_cast<glm::ivec2>(pos) / this->tile_size;

        return this->tiles.at((grid_pos.y * this->size.x) + grid_pos.x);
    }

    Map::Map(const glm::ivec2 &size, const glm::ivec2 &tile_size, const std::vector<std::shared_ptr<Layer>> &tiles,
             const std::vector<std::shared_ptr<Object>> &objects)
        : size(size), tile_size(tile_size), tiles(tiles), objects(objects) {
        for (auto &object : objects) {
            this->object_name_map.insert({object->name, std::weak_ptr<Object>(object)});
            this->object_type_map.insert({object->type, std::weak_ptr<Object>(object)});
        }
    }

    const glm::ivec2 &Map::get_size() {
        return this->size;
    }

    const glm::ivec2 &Map::get_tile_size() {
        return this->tile_size;
    }

    std::vector<std::shared_ptr<Tile>> Map::get_tiles(const glm::vec2 &pos) {
        std::vector<std::shared_ptr<Tile>> ret;

        for (auto &layer : this->tiles) {
            if (auto tile = layer->get_tile_at(pos)) {
                ret.push_back(tile);
            }
        }

        return ret;
    }

    std::shared_ptr<Map::Object> Map::get_object(const char *name, const char *type) {
        return nullptr;
    }

} // namespace milg

namespace milg {
    std::shared_ptr<Map::Layer> process_tile_layer(const nlohmann::json                 &json,
                                                   std::map<Gid, std::weak_ptr<Tileset>> tilesets,
                                                   const glm::ivec2                     &tile_size) {
        std::vector<std::shared_ptr<Tile>> tiles;
        glm::vec2                          offset;
        glm::ivec2                         layer_size;
        auto                               gids = json["data"].get<std::vector<Gid>>();

        json["x"].get_to(offset.x);
        json["y"].get_to(offset.y);

        json["width"].get_to(layer_size.x);
        json["height"].get_to(layer_size.y);

        for (int i = 0; i < layer_size.y; i++) {
            for (int j = 0; j < layer_size.x; j++) {
                auto gid     = gids[i * layer_size.y + j];
                auto tileset = tilesets[gid].lock();
                auto sprite  = milg::graphics::Sprite{
                     .position =
                         {
                            offset.x + (j * tile_size.x),
                            offset.y + (i * tile_size.y),
                        },
                     .size = tile_size,
                     .uvs  = tileset->get_uv(gid),
                };

                tiles.push_back(std::make_shared<Tile>(gid, sprite, tileset));
            }
        }

        auto name = json["name"].get<std::string>();

        return std::make_shared<Map::Layer>(offset, layer_size, tile_size, std::move(tiles));
    }

    std::vector<std::shared_ptr<Map::Object>> process_object_layer(const nlohmann::json &json) {
        std::vector<std::shared_ptr<Map::Object>> objects;

        for (auto &object_json : json["objects"]) {
            auto object = std::shared_ptr<Map::Object>(new Map::Object{
                .id   = object_json["id"].get<Map::Id>(),
                .name = object_json["name"].get<std::string>(),
                .type = object_json["type"].get<std::string>(),
                .pos =
                    {
                        object_json["x"].get<float>(),
                        object_json["y"].get<float>(),
                    },
                .size =
                    {
                        object_json["width"].get<float>(),
                        object_json["height"].get<float>(),
                    },
            });

            if (auto properties = object_json.find("properties"); properties != object_json.end()) {
                for (auto &property : *properties) {
                    auto name = property.at("name").get<std::string>();

                    object->properties[name] = property.at("value");
                }
            }

            objects.push_back(object);
        }

        return objects;
    }

    std::shared_ptr<void> Map::Loader::load(std::ifstream &stream) {
        auto                                  json = nlohmann::json::parse(stream);
        std::vector<std::shared_ptr<Tileset>> tilesets;
        std::map<Gid, std::weak_ptr<Tileset>> tileset_gid_map;

        for (auto &tileset_obj : json["tilesets"]) {
            auto first_gid    = tileset_obj.at("firstgid").get<Gid>();
            auto source       = tileset_obj.at("source").get<std::string>();
            auto loader_path  = this->get_current_path();
            auto tileset_json = AssetStore::load<nlohmann::json>(loader_path.replace_filename(source));
            auto texture_path = loader_path.parent_path().parent_path() / "textures";
            auto tileset      = std::make_shared<Tileset>(
                AssetStore::load<graphics::Texture>(texture_path / tileset_json->at("image").get<std::string>()),
                glm::ivec2{
                    tileset_json->at("tilewidth").get<int>(),
                    tileset_json->at("tileheight").get<int>(),
                },
                tileset_json->at("columns").get<std::size_t>(), tileset_json->at("spacing").get<std::size_t>(),
                tileset_json->at("margin").get<std::size_t>());

            tilesets.push_back(tileset);

            auto tile_count = tileset_json->at("tilecount").get<std::size_t>();

            for (auto i = first_gid; i <= first_gid + tile_count; i++) {
                tileset_gid_map[i] = tileset;
            }
        }

        std::vector<std::shared_ptr<Map::Layer>>  tiles;
        std::vector<std::shared_ptr<Map::Object>> objects;

        auto tile_size = glm::ivec2{
            json["tilewidth"].get<int>(),
            json["tileheight"].get<int>(),
        };

        for (auto &layer : json["layers"]) {
            auto type = layer["type"].get<Layer::Type>();

            switch (type) {
            case Layer::Type::TILE: {
                auto layer_tiles = process_tile_layer(layer, tileset_gid_map, tile_size);

                tiles.push_back(layer_tiles);
                break;
            }
            case Layer::Type::OBJECT: {
                auto layer_objects = process_object_layer(layer);

                objects.insert(objects.end(), layer_objects.begin(), layer_objects.end());
                break;
            }
            }
        }

        return std::make_shared<Map>(
            glm::ivec2{
                json["width"].get<int>(),
                json["height"].get<int>(),
            },
            tile_size, tiles, objects);
    }
} // namespace milg
