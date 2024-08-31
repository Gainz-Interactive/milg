#pragma once

#include <filesystem>
#include <format>
#include <stdexcept>

namespace milg {
    enum class asset_load_error {
        invalid_type,
        file_not_found,
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
