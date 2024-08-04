#pragma once

#include "logging.hpp"

#include <cstdint>
#include <memory>

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(func)                                                                                                 \
    do {                                                                                                               \
        VkResult result = func;                                                                                        \
        if (result != VK_SUCCESS) {                                                                                    \
            MILG_ERROR("{}:{} Vulkan error: {}", __FILE__, __LINE__, string_VkResult(result));                         \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (0)

namespace milg {
    class VulkanContext {
    public:
        static std::shared_ptr<VulkanContext> create(const std::unique_ptr<class Window> &window);

        ~VulkanContext();

        VkInstance             instance() const;
        VkPhysicalDevice       physical_device() const;
        VkDevice               device() const;
        const VolkDeviceTable &device_table() const;
        uint32_t               graphics_queue_family_index() const;
        VkQueue                graphics_queue() const;

        uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const;
        void     transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkFormat format,
                                         VkImageLayout old_layout, VkImageLayout new_layout) const;

    private:
        VulkanContext() = default;

        VkInstance                       m_instance                    = VK_NULL_HANDLE;
        VkPhysicalDevice                 m_physical_device             = VK_NULL_HANDLE;
        VkPhysicalDeviceMemoryProperties m_memory_properties           = {};
        VkDevice                         m_device                      = VK_NULL_HANDLE;
        VolkDeviceTable                  m_device_table                = {};
        uint32_t                         m_graphics_queue_family_index = 0;
        VkDebugUtilsMessengerEXT         m_debug_messenger             = VK_NULL_HANDLE;
        VkQueue                          m_graphics_queue              = VK_NULL_HANDLE;
    };
} // namespace milg
