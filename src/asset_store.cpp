#include "asset_store.hpp"
#include "error.hpp"
#include "logging.hpp"

#include <fstream>

namespace milg {
    void AssetStore::add_search_path(const std::string &path) {
        this->search_paths.push_back(std::filesystem::path(path).lexically_normal());
    }

    void AssetStore::load_assets(const std::string &path) {
        auto stream = std::ifstream(path);
        auto json   = nlohmann::json::parse(stream);

        for (const auto &[name, definition] : json.items()) {
            auto loaded = false;
            auto type   = definition["type"].template get<Asset::Type>();
            if (type == Asset::Type::INVALID) {
                type = Asset::Type::DATA;
            }

            if (!definition.contains("path")) {
                MILG_ERROR("Asset {} definition does not contain a path", name);

                continue;
            }

            MILG_DEBUG("Loading {}…", name);

            for (const auto &search_path : this->search_paths) {
                try {
                    auto path  = search_path / definition["path"];
                    auto asset = Asset::Loader::load(type, path);

                    this->assets.insert({name, asset});

                    loaded = true;

                    break;
                } catch (const milg::file_not_found_error &) {
                    continue;
                }
            }

            if (!loaded) {
                MILG_ERROR("Loading {} failed: file not found in any of the search paths", name);
            }
        }
    }

    void AssetStore::unload_assets() {
        this->assets.clear();
    }
} // namespace milg
