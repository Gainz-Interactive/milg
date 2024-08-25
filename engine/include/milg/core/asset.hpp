#pragma once

#include <any>
#include <cassert>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <variant>

namespace milg {
    class Asset {
    public:
        struct Bytes {
            char       *data;
            std::size_t size;
        };

        enum class Type {
            DATA,
            IMAGE,
            SOUND,
        };

        enum class Processor {
            NONE,
            JSON,
        };

        Asset() = delete;
        Asset(Type type, char *data, std::size_t size);
        Asset(Type type, std::any data);
        Asset(const Asset &) = delete;
        Asset(Asset &&)      = delete;

        Asset &operator=(const Asset &) = delete;
        Asset &operator=(Asset &&)      = delete;

        virtual ~Asset() = default;

        Type get_type();
        template <typename T> T &get() {
            assert(this->data.has_value());

            return std::any_cast<T &>(this->data);
        }
        Bytes &get_bytes();

        class Loader {
        public:
            static std::shared_ptr<Asset> load(Type type, const std::filesystem::path &path, Processor processor);
        };

    private:
        Type                          type;
        std::any data;
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Asset::Type, {
                                                  {Asset::Type::DATA, nullptr},
                                                  {Asset::Type::DATA, "data"},
                                                  {Asset::Type::IMAGE, "image"},
                                                  {Asset::Type::SOUND, "sound"},
                                              })
    NLOHMANN_JSON_SERIALIZE_ENUM(Asset::Processor, {
                                                          {Asset::Processor::NONE, nullptr},
                                                          {Asset::Processor::JSON, "json"},
                                                      })
} // namespace milg
