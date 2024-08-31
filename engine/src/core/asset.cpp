#include <milg/core/asset.hpp>
#include <nlohmann/json.hpp>

namespace milg {
    std::vector<std::filesystem::path>                        AssetStore::search_paths;
    std::map<std::type_index, std::shared_ptr<Asset::Loader>> AssetStore::loaders{
        {std::type_index(typeid(Bytes)), std::make_shared<Asset::Loader>()},
        {std::type_index(typeid(nlohmann::json)), std::make_shared<Asset::JsonLoader>()},
    };
    std::map<std::filesystem::path, std::shared_ptr<void>> AssetStore::assets;
} // namespace milg

namespace milg {
    auto Asset::Loader::load(std::ifstream &stream) -> LoadResult<void> {
        return std::make_shared<Bytes>(this->read_stream(stream));
    }

    const std::filesystem::path &Asset::Loader::get_current_path() {
        return this->path;
    }

    Bytes Asset::Loader::read_stream(std::ifstream &stream) {
        stream.seekg(0, std::ios::end);

        auto size = stream.tellg();
        auto data = std::vector<std::byte>(size);

        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char *>(data.data()), size);

        data.resize(stream.gcount());

        return data;
    }

    void Asset::Loader::set_current_path(const std::filesystem::path &path) {
        this->path = path;
    }

    auto Asset::JsonLoader::load(std::ifstream &stream) -> LoadResult<void> {
        auto json = std::make_shared<nlohmann::json>(nullptr);

        stream >> *json;

        return json;
    }
} // namespace milg

namespace milg {
    void AssetStore::add_search_path(const std::filesystem::path &path) {
        AssetStore::search_paths.push_back(path);
    }

    void AssetStore::unload_all() {
        AssetStore::assets.clear();
    }
} // namespace milg
