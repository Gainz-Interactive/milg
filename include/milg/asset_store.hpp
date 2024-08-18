#pragma once

#include "asset.hpp"

#include <filesystem>
#include <map>
#include <memory>

namespace milg::asset_store {
    void add_search_path(const std::filesystem::path &path);

    std::shared_ptr<Asset>                        get_asset(const std::string &name);
    std::map<std::string, std::shared_ptr<Asset>> get_assets(Asset::Type type);

    void load_asset(const std::string &name, const std::filesystem::path &path, Asset::Type type = Asset::Type::DATA,
                    Asset::Preprocessor preprocessor = Asset::Preprocessor::NONE);
    void load_assets(const std::filesystem::path &path);
    void unload_assets();
} // namespace milg::asset_store
