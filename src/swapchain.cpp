#include "swapchain.hpp"

#include "window.hpp"

namespace milg {
    std::shared_ptr<Swapchain> Swapchain::create(const std::unique_ptr<Window>        &window,
                                                 const std::shared_ptr<VulkanContext> &context) {

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
                if (format.format == VK_FORMAT_R8G8B8A8_UNORM) {
                    surface_format.format     = format.format;
                    surface_format.colorSpace = format.colorSpace;
                    break;
                }
            }

            if (surface_format.format == VK_FORMAT_UNDEFINED) {
                surface_format = formats[0];
            }
        }

        VkExtent2D extent = {
            .width  = window->width(),
            .height = window->height(),
        };

        const VkSwapchainCreateInfoKHR swapchain_info = {
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = surface,
            .minImageCount         = surface_capabilities.minImageCount,
            .imageFormat           = surface_format.format,
            .imageColorSpace       = surface_format.colorSpace,
            .imageExtent           = extent,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode           = VK_PRESENT_MODE_FIFO_KHR,
            .clipped               = VK_TRUE,
            .oldSwapchain          = VK_NULL_HANDLE,
        };

        VkSwapchainKHR swapchain_handle = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateSwapchainKHR(context->device(), &swapchain_info, nullptr,
                                                              &swapchain_handle));

        uint32_t image_count = 0u;
        VK_CHECK(context->device_table().vkGetSwapchainImagesKHR(context->device(), swapchain_handle, &image_count,
                                                                 nullptr));

        std::vector<VkImage> swapchain_image_handles(image_count);
        VK_CHECK(context->device_table().vkGetSwapchainImagesKHR(context->device(), swapchain_handle, &image_count,
                                                                 swapchain_image_handles.data()));

        std::vector<VkImageView> swapchain_image_views(image_count);
        for (size_t i = 0; i < image_count; ++i) {
            const VkImageViewCreateInfo image_view_info = {
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .image    = swapchain_image_handles[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = surface_format.format,
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

            VK_CHECK(context->device_table().vkCreateImageView(context->device(), &image_view_info, nullptr,
                                                               &swapchain_image_views[i]));
        }

        std::vector<SwapchainImage> swapchain_images;
        for (size_t i = 0; i < image_count; ++i) {
            swapchain_images.push_back(SwapchainImage{
                .image = swapchain_image_handles[i],
                .view  = swapchain_image_views[i],
            });
        }

        auto swapchain              = std::shared_ptr<Swapchain>(new Swapchain());
        swapchain->m_context        = context;
        swapchain->m_swapchain      = swapchain_handle;
        swapchain->m_surface        = surface;
        swapchain->m_surface_format = surface_format;
        swapchain->m_present_mode   = VK_PRESENT_MODE_FIFO_KHR;
        swapchain->m_extent         = extent;
        swapchain->m_images         = swapchain_images;

        return swapchain;
    }

    Swapchain::~Swapchain() {
        for (const auto &image : m_images) {
            vkDestroyImageView(m_context->device(), image.view, nullptr);
        }

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
} // namespace milg
