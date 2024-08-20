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
} // namespace milg
