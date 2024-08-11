#pragma once

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>

namespace milg {
    class Asset {
    public:
        enum class Type {
            DATA,
            IMAGE,
            SOUND,

            INVALID = -1,
        };
        Asset() = delete;
        Asset(Type type, char *data, std::size_t size);
        Asset(const Asset &) = delete;
        Asset(Asset &&)      = delete;

        Asset &operator=(const Asset &) = delete;
        Asset &operator=(Asset &&)      = delete;

        virtual ~Asset();

        char       *get_data();
        std::size_t get_size();
        Type        get_type();

        class Loader {
        public:
            static std::shared_ptr<Asset> load(Type type, const std::filesystem::path &path);
        };

    private:
        char       *data;
        std::size_t size;
        Type        type;
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Asset::Type, {
                                                  {Asset::Type::INVALID, nullptr},
                                                  {Asset::Type::DATA, "data"},
                                                  {Asset::Type::IMAGE, "image"},
                                                  {Asset::Type::SOUND, "sound"},
                                              })
} // namespace milg
