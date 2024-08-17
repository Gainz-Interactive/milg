#include "buffer.hpp"

namespace milg::graphics {
    std::shared_ptr<Buffer> Buffer::create(const std::shared_ptr<VulkanContext> &context,
                                           const BufferCreateInfo               &create_info) {
        const VkBufferCreateInfo buffer_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .size                  = create_info.size,
            .usage                 = create_info.usage_flags,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
        };

        const VmaAllocationCreateInfo allocation_create_info = {
            .flags          = create_info.allocation_flags,
            .usage          = create_info.memory_usage,
            .requiredFlags  = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = nullptr,
            .pUserData      = nullptr,
            .priority       = 0.0f,
        };

        VkBuffer          buffer_handle   = VK_NULL_HANDLE;
        VmaAllocation     allocation      = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info = {};
        VK_CHECK(vmaCreateBuffer(context->allocator(), &buffer_create_info, &allocation_create_info, &buffer_handle,
                                 &allocation, &allocation_info));

        auto buffer               = std::shared_ptr<Buffer>(new Buffer());
        buffer->m_context         = context;
        buffer->m_size            = create_info.size;
        buffer->m_handle          = buffer_handle;
        buffer->m_allocation      = allocation;
        buffer->m_allocation_info = allocation_info;
        buffer->m_usage_flags     = create_info.usage_flags;

        return buffer;
    }

    VkDeviceSize Buffer::size() const {
        return m_size;
    }

    VkBuffer Buffer::handle() const {
        return m_handle;
    }

    VmaAllocation Buffer::allocation() const {
        return m_allocation;
    }

    VmaAllocationInfo Buffer::allocation_info() const {
        return m_allocation_info;
    }

    VkBufferUsageFlags Buffer::usage_flags() const {
        return m_usage_flags;
    }

    Buffer::~Buffer() {
        vmaDestroyBuffer(m_context->allocator(), m_handle, m_allocation);
    }

} // namespace milg::graphics
