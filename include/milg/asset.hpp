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
        enum class Type {
            DATA,
            IMAGE,
            SOUND,
        };

        enum class Preprocessor {
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

        /**
         * Get unprocessed baitų šmotas.
         * Will be null if the data has been preprocessed.
         */
        char *get_data();
        /**
         * Get size of unprocessed baitų šmotas.
         * Will be 0 if the data has been preprocessed.
         */
        std::size_t get_size();

        Type get_type();
        /**
         * Get preprocessed data if a preprocessor was run.
         * If no preprocessor was run, the object will have no value.
         */
        template <typename T> T &get_preprocessed() {
            assert(std::holds_alternative<std::any>(this->data));

            auto &any = std::get<std::any>(this->data);

            assert(any.has_value());

            return std::any_cast<T &>(any);
        }

        class Loader {
        public:
            static std::shared_ptr<Asset> load(Type type, const std::filesystem::path &path, Preprocessor preprocessor);
        };

    private:
        struct Bytes {
            char       *data;
            std::size_t size;
        };

        Type                          type;
        std::variant<Bytes, std::any> data;
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(Asset::Type, {
                                                  {Asset::Type::DATA, nullptr},
                                                  {Asset::Type::DATA, "data"},
                                                  {Asset::Type::IMAGE, "image"},
                                                  {Asset::Type::SOUND, "sound"},
                                              })
    NLOHMANN_JSON_SERIALIZE_ENUM(Asset::Preprocessor, {
                                                          {Asset::Preprocessor::NONE, nullptr},
                                                          {Asset::Preprocessor::JSON, "json"},
                                                      })
} // namespace milg
