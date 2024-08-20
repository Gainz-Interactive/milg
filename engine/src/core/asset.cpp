#include <milg/core/asset.hpp>
#include <milg/core/error.hpp>
#include <milg/core/logging.hpp>

#include <fstream>

namespace milg {
    Asset::Asset(Type type, char *data, std::size_t size) : type(type), data(Bytes{data, size}) {
    }

    Asset::Asset(Type type, std::any data) : type(type), data(data) {
    }

    char *Asset::get_data() {
        if (!std::holds_alternative<Bytes>(this->data)) {
            return nullptr;
        }

        auto &bytes = std::get<Bytes>(this->data);

        return bytes.data;
    }

    std::size_t Asset::get_size() {
        if (!std::holds_alternative<Bytes>(this->data)) {
            return 0;
        }

        auto &bytes = std::get<Bytes>(this->data);

        return bytes.size;
    }

    Asset::Type Asset::get_type() {
        return this->type;
    }

    static std::shared_ptr<Asset> preprocess(std::ifstream &stream, Asset::Preprocessor preprocessor,
                                             Asset::Type type) {
        // These preprocessors can consume streams.
        switch (preprocessor) {
        case Asset::Preprocessor::JSON:
            return std::make_shared<Asset>(type, std::any(nlohmann::json::parse(stream)));
        default:
            break;
        }

        stream.seekg(0, std::ios::end);

        auto size = stream.tellg();
        auto data = new char[size];

        stream.seekg(0, std::ios::beg);
        stream.read(data, size);

        // These preprocessors require a baitų šmotas.
        switch (preprocessor) {
        default:
            break;
        }

        return std::make_shared<Asset>(type, data, size);
    }

    std::shared_ptr<Asset> Asset::Loader::load(Type type, const std::filesystem::path &path,
                                               Preprocessor preprocessor) {
        std::ifstream stream(path, std::ios::binary | std::ios::in);
        if (!stream.is_open()) {
            throw file_not_found_error(path);
        }

        return preprocess(stream, preprocessor, type);
    }
} // namespace milg
