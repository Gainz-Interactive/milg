#include <fstream>
#include <milg/asset_store.hpp>
#include <milg/error.hpp>
#include <milg/logging.hpp>

std::map<std::string, std::shared_ptr<milg::Asset>> assets;
std::vector<std::filesystem::path>                  search_paths;

namespace milg::asset_store {
    void add_search_path(const std::filesystem::path &path) {
        search_paths.push_back(path);
    }

    std::shared_ptr<milg::Asset> get_asset(const std::string &name) {
        auto iter = assets.find(name);
        if (iter == assets.end()) {
            return nullptr;
        }

        return iter->second;
    }

    std::map<std::string, std::shared_ptr<milg::Asset>> get_assets(milg::Asset::Type type) {
        std::map<std::string, std::shared_ptr<milg::Asset>> ret;

        for (auto &[name, asset] : assets) {
            if (asset->get_type() == type) {
                ret[name] = asset;
            }
        }

        return ret;
    }

    void load_asset(const std::string &name, const std::filesystem::path &path, Asset::Type type,
                    Asset::Preprocessor preprocessor) {
        for (const auto &search_path : search_paths) {
            try {
                assets.insert({name, Asset::Loader::load(type, search_path / path, preprocessor)});

                return;
            } catch (const milg::file_not_found_error &) {
                continue;
            }
        }

        throw file_not_found_error(path);
    }

    void load_assets(const std::filesystem::path &path) {
        auto stream = std::ifstream(path);
        auto json   = nlohmann::json::parse(stream);

        for (const auto &[name, definition] : json.items()) {
            auto type         = definition["type"].template get<Asset::Type>();
            auto preprocessor = definition["preprocess"].template get<Asset::Preprocessor>();

            if (!definition.contains("path")) {
                MILG_ERROR("Asset {} definition does not contain a path", name);

                continue;
            }

            MILG_DEBUG("Loading {}…", name);

            try {
                load_asset(name, definition["path"], type, preprocessor);
            } catch (const milg::file_not_found_error &) {
                MILG_ERROR("Loading {} failed: file not found in any of the search paths", name);
            }
        }
    }

    void unload_assets() {
        assets.clear();
    }
} // namespace milg::asset_store
