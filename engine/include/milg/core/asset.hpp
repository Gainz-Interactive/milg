#pragma once

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <milg/core/error.hpp>
#include <milg/core/logging.hpp>
#include <milg/core/types.hpp>
#include <typeindex>

namespace milg {
    class Asset {
    public:
        class Loader {
        public:
            virtual std::shared_ptr<void> load(std::ifstream &stream);

        protected:
            const std::filesystem::path &get_current_path();
            Bytes                        read_stream(std::ifstream &stream);

        private:
            friend class AssetStore;

            void set_current_path(const std::filesystem::path &path);

            std::filesystem::path path;
        };

        class JsonLoader : public Loader {
        public:
            std::shared_ptr<void> load(std::ifstream &stream);
        };
    };

    class AssetStore {
    public:
        static void add_search_path(const std::filesystem::path &path);

        template <typename T> static std::shared_ptr<T> load(const std::filesystem::path &path) {
            if (auto iter = AssetStore::assets.find(path); iter != AssetStore::assets.end()) {
                return std::static_pointer_cast<T>(iter->second);
            }

            std::shared_ptr<Asset::Loader> loader = nullptr;

            if (auto iter = AssetStore::loaders.find(std::type_index(typeid(T))); iter != loaders.end()) {
                loader = iter->second;
            } else {
                throw invalid_asset_type_error();
            }

            MILG_DEBUG("Loading {}…", path.string());

            for (const auto &search_path : AssetStore::search_paths) {
                try {
                    auto          current_path = search_path / path;
                    std::ifstream stream(current_path, std::ios::binary | std::ios::in);
                    if (!stream.is_open()) {
                        throw file_not_found_error(path);
                    }

                    loader->set_current_path(current_path);

                    auto data = loader->load(stream);

                    assets.insert({path, data});

                    return std::static_pointer_cast<T>(data);
                } catch (const milg::file_not_found_error &) {
                    continue;
                }
            }

            throw file_not_found_error(path);

            return nullptr;
        }
        static void unload_all();

        template <typename T> static void register_loader(std::shared_ptr<Asset::Loader> loader) {
            AssetStore::loaders[std::type_index(typeid(T))] = loader;
        }

    private:
        static std::vector<std::filesystem::path>                        search_paths;
        static std::map<std::type_index, std::shared_ptr<Asset::Loader>> loaders;
        static std::map<std::filesystem::path, std::shared_ptr<void>>    assets;
    };
} // namespace milg
