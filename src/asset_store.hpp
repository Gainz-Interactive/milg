#pragma once

#include "asset.hpp"

#include <filesystem>
#include <map>
#include <memory>

namespace milg {
    class AssetStore {
    public:
        AssetStore()                   = default;
        AssetStore(const AssetStore &) = delete;
        AssetStore(AssetStore &&)      = default;

        AssetStore &operator=(const AssetStore &) = delete;
        AssetStore &operator=(AssetStore &&)      = default;

        ~AssetStore() = default;

        void add_search_path(const std::string &path);

        std::shared_ptr<Asset> get_asset(const std::string &name) {
            auto iter = this->assets.find(name);
            if (iter == this->assets.end()) {
                return nullptr;
            }

            return iter->second;
        }
        std::map<std::string, std::shared_ptr<Asset>> get_assets(Asset::Type type) {
            std::map<std::string, std::shared_ptr<Asset>> ret;

            for (auto &[name, asset] : this->assets) {
                if (asset->get_type() == type) {
                    ret[name] = asset;
                }
            }

            return ret;
        }

        void load_assets(const std::string &path);
        void unload_assets();

    private:
        std::map<std::string, std::shared_ptr<Asset>> assets;
        std::vector<std::filesystem::path>            search_paths;
    };
} // namespace milg
