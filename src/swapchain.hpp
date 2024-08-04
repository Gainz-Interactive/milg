#pragma once

#include "vk_context.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace milg {
    struct SwapchainImage {
        VkImage     image = VK_NULL_HANDLE;
        VkImageView view  = VK_NULL_HANDLE;
    };

    class Swapchain {
    public:
        static std::shared_ptr<Swapchain> create(const std::unique_ptr<class Window>  &window,
                                                 const std::shared_ptr<VulkanContext> &context);

        ~Swapchain();

        VkSurfaceFormatKHR surface_format() const;
        VkSwapchainKHR     handle() const;
        VkExtent2D         extent() const;

        SwapchainImage &get_image(uint32_t index);

        uint32_t image_count() const;

    private:
        std::shared_ptr<VulkanContext> m_context;

        VkSwapchainKHR              m_swapchain      = VK_NULL_HANDLE;
        VkSurfaceKHR                m_surface        = VK_NULL_HANDLE;
        VkSurfaceFormatKHR          m_surface_format = {};
        VkPresentModeKHR            m_present_mode   = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D                  m_extent         = {};
        std::vector<SwapchainImage> m_images;

        Swapchain() = default;
    };
} // namespace milg
