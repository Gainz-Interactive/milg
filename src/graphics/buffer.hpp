#pragma once

#include "vk_context.hpp"

#include <memory>

namespace milg::graphics {
    struct BufferCreateInfo {
        VkDeviceSize             size             = 0;
        VmaMemoryUsage           memory_usage     = VMA_MEMORY_USAGE_UNKNOWN;
        VmaAllocationCreateFlags allocation_flags = 0;
        VkBufferUsageFlags       usage_flags      = 0;
    };

    class Buffer {
    public:
        static std::shared_ptr<Buffer> create(const std::shared_ptr<VulkanContext> &context,
                                              const BufferCreateInfo               &create_info);

        ~Buffer();

        VkDeviceSize       size() const;
        VkBuffer           handle() const;
        VmaAllocation      allocation() const;
        VmaAllocationInfo  allocation_info() const;
        VkBufferUsageFlags usage_flags() const;

    private:
        std::shared_ptr<VulkanContext> m_context = nullptr;

        VkDeviceSize       m_size            = 0;
        VkBuffer           m_handle          = VK_NULL_HANDLE;
        VkBufferUsageFlags m_usage_flags     = 0;
        VmaAllocation      m_allocation      = VK_NULL_HANDLE;
        VmaAllocationInfo  m_allocation_info = {};

        Buffer() = default;
    };
} // namespace milg::graphics
