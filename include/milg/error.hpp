#pragma once

#include <format>
#include <stdexcept>

namespace milg {
    class file_not_found_error : public std::runtime_error {
    public:
        explicit file_not_found_error(const std::string &path)
            : std::runtime_error(std::format("file {} not found", path)), path(path) {
        }

    private:
        std::string path;
    };
} // namespace milg
