#pragma once

#include <filesystem>
#include <format>
#include <stdexcept>

namespace milg {
    class file_not_found_error : public std::runtime_error {
    public:
        explicit file_not_found_error(const std::filesystem::path &path)
            : std::runtime_error(std::format("file {} not found", path.string())), path(path) {
        }

    private:
        std::filesystem::path path;
    };

    class invalid_asset_type_error : public std::runtime_error {
    public:
        invalid_asset_type_error() : std::runtime_error("no asset loader exists for requested type") {
        }
    };

    class vulkan_context_error {
    public:
        class destroyed : public std::runtime_error {
        public:
            destroyed() : std::runtime_error("Vulkan context was destroyed unexpectedly") {
            }
        };
    };
} // namespace milg
