#include "swapchain.hpp"

#include "window.hpp"
#include <cstdint>
#include <milg.hpp>

namespace milg::graphics {
    std::shared_ptr<Swapchain> Swapchain::create(const std::unique_ptr<Window>        &window,
                                                 const std::shared_ptr<VulkanContext> &context) {
        MILG_INFO("Creating swapchain");
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        window->get_swapchain_surface(context, &surface);

        VkSurfaceCapabilitiesKHR surface_capabilities;
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device(), surface, &surface_capabilities));

        uint32_t format_count = 0u;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device(), surface, &format_count, nullptr));

        std::vector<VkSurfaceFormatKHR> formats(format_count);
        VK_CHECK(
            vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device(), surface, &format_count, formats.data()));

        VkSurfaceFormatKHR surface_format = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
            surface_format.format     = VK_FORMAT_R8G8B8A8_UNORM;
            surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        } else {
            for (const auto &format : formats) {
                if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    surface_format.format     = format.format;
                    surface_format.colorSpace = format.colorSpace;
                    break;
                }
            }

            if (surface_format.format == VK_FORMAT_UNDEFINED) {
                surface_format = formats[0];
            }
        }

        MILG_INFO("Selected surface format: {}, colorspace: {}", string_VkFormat(surface_format.format),
                  string_VkColorSpaceKHR(surface_format.colorSpace));

        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

        MILG_INFO("Selected present mode: {}", string_VkPresentModeKHR(present_mode));

        auto swapchain                    = std::shared_ptr<Swapchain>(new Swapchain());
        swapchain->m_context              = context;
        swapchain->m_surface              = surface;
        swapchain->m_surface_format       = surface_format;
        swapchain->m_present_mode         = present_mode;
        swapchain->m_surface_capabilities = surface_capabilities;
        swapchain->resize(window->width(), window->height());

        return swapchain;
    }

    void Swapchain::resize(uint32_t width, uint32_t height) {
        cleanup();

        VkExtent2D extent = {
            .width  = width,
            .height = height,
        };

        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = m_surface,
            .minImageCount         = m_surface_capabilities.minImageCount,
            .imageFormat           = m_surface_format.format,
            .imageColorSpace       = m_surface_format.colorSpace,
            .imageExtent           = extent,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode           = m_present_mode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = m_swapchain,
        };

        VK_CHECK(m_context->device_table().vkCreateSwapchainKHR(m_context->device(), &swapchain_info, nullptr,
                                                                &m_swapchain));

        uint32_t image_count = 0u;
        VK_CHECK(
            m_context->device_table().vkGetSwapchainImagesKHR(m_context->device(), m_swapchain, &image_count, nullptr));

        std::vector<VkImage> swapchain_image_handles(image_count);
        VK_CHECK(m_context->device_table().vkGetSwapchainImagesKHR(m_context->device(), m_swapchain, &image_count,
                                                                   swapchain_image_handles.data()));

        std::vector<VkImageView> swapchain_image_views(image_count);
        for (size_t i = 0; i < image_count; ++i) {
            const VkImageViewCreateInfo image_view_info = {
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .image    = swapchain_image_handles[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = m_surface_format.format,
                .components =
                    {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                .subresourceRange =
                    {
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
            };

            VK_CHECK(m_context->device_table().vkCreateImageView(m_context->device(), &image_view_info, nullptr,
                                                                 &swapchain_image_views[i]));
        }

        for (size_t i = 0; i < image_count; ++i) {
            m_images.push_back(SwapchainImage{
                .image  = swapchain_image_handles[i],
                .view   = swapchain_image_views[i],
                .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            });
        }

        m_extent.width  = width;
        m_extent.height = height;
    }

    uint32_t Swapchain::acquire_next_image(VkSemaphore semaphore, VkFence fence) {
        (vkAcquireNextImageKHR(m_context->device(), m_swapchain, UINT64_MAX, semaphore, fence, &m_image_index));
        m_images[m_image_index].layout = VK_IMAGE_LAYOUT_UNDEFINED;

        return m_image_index;
    }

    void Swapchain::present_image(VkQueue queue, VkSemaphore semaphore) {
        VkPresentInfoKHR present_info = {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &m_swapchain,
            .pImageIndices      = &m_image_index,
            .pResults           = nullptr,
        };

        (vkQueuePresentKHR(queue, &present_info));
    }

    void Swapchain::transition_current_image(VkCommandBuffer command_buffer, VkImageLayout new_layout) {
        SwapchainImage &image = m_images[m_image_index];

        m_context->transition_image_layout(command_buffer, image.image, image.layout, new_layout);
        image.layout = new_layout;
    }

    void Swapchain::blit_to_current_image(VkCommandBuffer command_buffer, VkImage image, VkExtent2D extent) {
        VkImageBlit2 blit_region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            .srcOffsets =
                {
                    {0, 0, 0},
                    {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1},
                },
            .dstSubresource =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            .dstOffsets =
                {
                    {0, 0, 0},
                    {static_cast<int32_t>(m_extent.width), static_cast<int32_t>(m_extent.height), 1},
                },
        };

        VkBlitImageInfo2 blit_info = {
            .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = m_images[m_image_index].image,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &blit_region,
            .filter         = VK_FILTER_LINEAR,
        };

        vkCmdBlitImage2(command_buffer, &blit_info);
    }

    uint32_t Swapchain::current_image_index() const {
        return m_image_index;
    }

    void Swapchain::cleanup() {
        for (const auto &image : m_images) {
            vkDestroyImageView(m_context->device(), image.view, nullptr);
        }
        m_images.clear();
    }

    Swapchain::~Swapchain() {
        cleanup();

        vkDestroySwapchainKHR(m_context->device(), m_swapchain, nullptr);
        vkDestroySurfaceKHR(m_context->instance(), m_surface, nullptr);
    }

    VkSurfaceFormatKHR Swapchain::surface_format() const {
        return m_surface_format;
    }

    VkSwapchainKHR Swapchain::handle() const {
        return m_swapchain;
    }

    SwapchainImage &Swapchain::get_image(uint32_t index) {
        return m_images[index];
    }

    VkExtent2D Swapchain::extent() const {
        return m_extent;
    }

    uint32_t Swapchain::image_count() const {
        return static_cast<uint32_t>(m_images.size());
    }
} // namespace milg::graphics
