#include <milg/core/asset.hpp>
#include <milg/core/error.hpp>
#include <milg/core/logging.hpp>

#include <fstream>

namespace milg {
    Asset::Asset(Type type, char *data, std::size_t size) : type(type), data(Asset::Bytes{data, size}) {
    }

    Asset::Asset(Type type, std::any data) : type(type), data(data) {
    }

    Asset::Type Asset::get_type() {
        return this->type;
    }

    Asset::Bytes &Asset::get_bytes() {
        return this->get<Bytes>();
    }

    static std::shared_ptr<Asset> process(std::ifstream &stream, Asset::Processor processor,
                                             Asset::Type type) {
        // These processors can consume streams.
        switch (processor) {
        case Asset::Processor::JSON:
            return std::make_shared<Asset>(type, std::any(nlohmann::json::parse(stream)));
        default:
            break;
        }

        stream.seekg(0, std::ios::end);

        auto size = stream.tellg();
        auto data = new char[size];

        stream.seekg(0, std::ios::beg);
        stream.read(data, size);

        // These processors require a baitų šmotas.
        switch (processor) {
        default:
            break;
        }

        return std::make_shared<Asset>(type, data, size);
    }

    std::shared_ptr<Asset> Asset::Loader::load(Type type, const std::filesystem::path &path,
                                               Processor processor) {
        std::ifstream stream(path, std::ios::binary | std::ios::in);
        if (!stream.is_open()) {
            throw file_not_found_error(path);
        }

        return process(stream, processor, type);
    }
} // namespace milg
