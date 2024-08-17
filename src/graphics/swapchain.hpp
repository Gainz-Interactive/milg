#pragma once

#include "vk_context.hpp"
#include "window.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace milg::graphics {
    struct SwapchainImage {
        VkImage       image  = VK_NULL_HANDLE;
        VkImageView   view   = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    class Swapchain {
    public:
        static std::shared_ptr<Swapchain> create(const std::unique_ptr<Window>        &window,
                                                 const std::shared_ptr<VulkanContext> &context);

        ~Swapchain();

        VkSurfaceFormatKHR surface_format() const;
        VkSwapchainKHR     handle() const;
        VkExtent2D         extent() const;

        SwapchainImage &get_image(uint32_t index);
        uint32_t        acquire_next_image(VkSemaphore semaphore, VkFence fence);
        void            present_image(VkQueue queue, VkSemaphore semaphore);
        void            transition_current_image(VkCommandBuffer command_buffer, VkImageLayout new_layout);
        void            blit_to_current_image(VkCommandBuffer command_buffer, VkImage image, VkExtent2D extent);
        uint32_t        current_image_index() const;

        void resize(uint32_t width, uint32_t height);

        uint32_t image_count() const;

    private:
        std::shared_ptr<VulkanContext> m_context;

        VkSwapchainKHR              m_swapchain            = VK_NULL_HANDLE;
        VkSurfaceKHR                m_surface              = VK_NULL_HANDLE;
        VkSurfaceFormatKHR          m_surface_format       = {};
        VkPresentModeKHR            m_present_mode         = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D                  m_extent               = {};
        VkSurfaceCapabilitiesKHR    m_surface_capabilities = {};
        std::vector<SwapchainImage> m_images;
        uint32_t                    m_image_index = 0;

        void cleanup();

        Swapchain() = default;
    };
} // namespace milg::graphics
