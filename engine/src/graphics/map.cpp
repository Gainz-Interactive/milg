#include <milg/graphics/map.hpp>

#include <milg/core/asset_store.hpp>
#include <milg/core/logging.hpp>

#include <stb_image.h>

namespace milg {
    Tileset::Tileset(const nlohmann::json &json) {
        json["name"].get_to(this->name);
        json["tilecount"].get_to(this->tile_count);
        json["imagewidth"].get_to(this->width);
        json["imageheight"].get_to(this->height);
        json["tilewidth"].get_to(this->tile_width);
        json["tileheight"].get_to(this->tile_height);
        json["columns"].get_to(this->columns);
        json["spacing"].get_to(this->spacing);
        json["margin"].get_to(this->margin);
        json["image"].get_to(this->source);
    }

    const std::string &Tileset::get_name() {
        return this->name;
    }

    const std::filesystem::path &Tileset::get_source() {
        return this->source;
    }

    std::size_t Tileset::get_tile_count() {
        return this->tile_count;
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

    Map::Map(const nlohmann::json &json) {
        json["width"].get_to(this->width);
        json["height"].get_to(this->height);

        json["tilewidth"].get_to(this->tile_width);
        json["tileheight"].get_to(this->tile_height);

        std::map<Gid, std::weak_ptr<Tileset>> tileset_gid_map;

        for (auto &tileset_json : json["tilesets"]) {
            auto first_gid = tileset_json.at("firstgid").get<Gid>();
            auto source    = tileset_json.at("source").get<std::string>();
            auto asset     = asset_store::get_asset(source);
            auto tileset   = std::make_shared<Tileset>(asset->get_preprocessed<nlohmann::json>());

            tilesets.push_back(tileset);

            for (auto i = first_gid; i <= first_gid + tileset->get_tile_count(); i++) {
                tileset_gid_map[i] = tileset;
            }
        }

        for (auto &layer : json["layers"]) {
            auto type = layer["type"].get<LayerType>();

            switch (type) {
            case LayerType::TILE:
                this->process_tile_layer(tileset_gid_map, layer);
                break;
            case LayerType::OBJECT:
                this->process_object_layer(layer);
                break;
            }
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

    std::vector<Tile> *Map::get_layer(const std::string &name) {
        if (auto iter = this->layers.find(name); iter != this->layers.end()) {
            return &iter->second;
        }

        return nullptr;
    }

    std::shared_ptr<Map::Object> Map::get_object(const char *name, const char *type) {
        return nullptr;
    }

    std::vector<std::shared_ptr<Tileset>> Map::get_tilesets() {
        return this->tilesets;
    }

    void Map::process_tile_layer(std::map<Gid, std::weak_ptr<Tileset>> tilesets, const nlohmann::json &json) {
        std::vector<Tile> tiles;
        auto              gids         = json["data"].get<std::vector<Gid>>();
        auto              x_offset     = json["x"].get<std::size_t>();
        auto              y_offset     = json["y"].get<std::size_t>();
        auto              layer_width  = json["width"].get<Gid>();
        auto              layer_height = json["height"].get<Gid>();

        for (Gid i = 0; i < layer_height; i++) {
            for (Gid j = 0; j < layer_width; j++) {
                auto gid     = gids[i * layer_height + j];
                auto tileset = tilesets[gid].lock();
                auto sprite  = milg::graphics::Sprite{
                     .position =
                         {
                            (x_offset * this->tile_width) + (j * this->tile_width),
                            (y_offset * this->tile_height) + (i * this->tile_height),
                        },
                     .size = {this->tile_width, this->tile_height},
                     .uvs  = tileset->get_uv(gid),
                };

                tiles.emplace_back(gid, sprite, tileset);
            }
        }

        auto name = json["name"].get<std::string>();

        this->layers[name] = tiles;
    }

    void Map::process_object_layer(const nlohmann::json &json) {
        for (auto &object_json : json["objects"]) {
            auto name   = object_json["name"].get<std::string>();
            auto type   = object_json["type"].get<std::string>();
            auto object = std::shared_ptr<Object>(new Object{
                .id   = object_json["id"].get<Id>(),
                .name = name,
                .type = type,
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

            this->objects.push_back(object);

            this->object_name_map.insert({name, std::weak_ptr<Object>(object)});
            this->object_type_map.insert({type, std::weak_ptr<Object>(object)});
        }
    }
} // namespace milg
