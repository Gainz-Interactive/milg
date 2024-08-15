#pragma once

#include "asset.hpp"

#include <filesystem>
#include <map>
#include <memory>

namespace milg::asset_store {
    void add_search_path(const std::filesystem::path &path);

    std::shared_ptr<Asset>                        get_asset(const std::string &name);
    std::map<std::string, std::shared_ptr<Asset>> get_assets(Asset::Type type);

    void load_assets(const std::string &path);
    void unload_assets();
} // namespace milg::asset_store
