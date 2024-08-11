#include "asset.hpp"
#include "error.hpp"

#include <fstream>

namespace milg {
    Asset::Asset(Type type, char *data, std::size_t size) : type(type), data(data), size(size) {
    }

    Asset::~Asset() {
        delete[] this->data;
    }

    char *Asset::get_data() {
        return this->data;
    }

    std::size_t Asset::get_size() {
        return this->size;
    }

    Asset::Type Asset::get_type() {
        return this->type;
    }

    std::shared_ptr<Asset> Asset::Loader::load(Type type, const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary | std::ios::in | std::ios::ate);
        if (!file.is_open()) {
            throw file_not_found_error(path);
        }
        auto size = file.tellg();
        auto data = new char[size];

        file.seekg(0, std::ios::beg);
        file.read(data, size);

        return std::make_shared<Asset>(type, data, size);
    }
} // namespace milg
