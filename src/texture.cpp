#include "texture.hpp"

#include "logging.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdint>

namespace milg {
    std::shared_ptr<Texture> Texture::load_from_file(const std::shared_ptr<VulkanContext> &context,
                                                     const TextureCreateInfo              &create_info,
                                                     const std::filesystem::path          &path) {
        MILG_INFO("Loading texture from file: {}", path.string());

        int32_t  width    = 0;
        int32_t  height   = 0;
        int32_t  channels = 0;
        stbi_uc *data     = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data) {
            MILG_ERROR("Failed to read texture file {}: {}", path.string(), stbi_failure_reason());
            return nullptr;
        }

        VkImageUsageFlags usage_flags = create_info.usage;
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkImageCreateInfo image_create_info = {
            .sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext     = nullptr,
            .flags     = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format    = create_info.format,
            .extent =
                {
                    .width  = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth  = 1,
                },
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = usage_flags,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        const VmaAllocationCreateInfo allocation_create_info = {
            .flags          = 0,
            .usage          = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags  = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = VK_NULL_HANDLE,
            .pUserData      = nullptr,
            .priority       = 0.0f,
        };

        VkImage           image           = VK_NULL_HANDLE;
        VmaAllocation     allocation      = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info = {};
        VK_CHECK(vmaCreateImage(context->allocator(), &image_create_info, &allocation_create_info, &image, &allocation,
                                &allocation_info));

        const VkBufferCreateInfo staging_buffer_create_info = {
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .size                  = static_cast<size_t>(width * height * 4),
            .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
        };

        const VmaAllocationCreateInfo staging_allocation_create_info = {
            .flags          = 0,
            .usage          = VMA_MEMORY_USAGE_CPU_ONLY,
            .requiredFlags  = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = VK_NULL_HANDLE,
            .pUserData      = nullptr,
            .priority       = 0.0f,
        };

        VkBuffer          staging_buffer          = VK_NULL_HANDLE;
        VmaAllocation     staging_allocation      = VK_NULL_HANDLE;
        VmaAllocationInfo staging_allocation_info = {};
        VK_CHECK(vmaCreateBuffer(context->allocator(), &staging_buffer_create_info, &staging_allocation_create_info,
                                 &staging_buffer, &staging_allocation, &staging_allocation_info));

        void *staging_data = nullptr;
        VK_CHECK(vmaMapMemory(context->allocator(), staging_allocation, &staging_data));
        memcpy(staging_data, data, static_cast<size_t>(width * height) * 4);
        vmaUnmapMemory(context->allocator(), staging_allocation);

        stbi_image_free(data);

        VkCommandBuffer command_buffer = context->begin_single_time_commands();

        VkImageMemoryBarrier barrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext               = nullptr,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
        };

        context->device_table().vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                     VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                                     &barrier);

        const VkBufferImageCopy copy_region = {
            .bufferOffset      = 0,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            .imageOffset =
                {
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
            .imageExtent =
                {
                    .width  = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth  = 1,
                },
        };

        context->device_table().vkCmdCopyBufferToImage(command_buffer, staging_buffer, image,
                                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        context->device_table().vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                                                     1, &barrier);
        context->end_single_time_commands(command_buffer);
        vmaDestroyBuffer(context->allocator(), staging_buffer, staging_allocation);

        VkImageLayout image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        const VkImageViewCreateInfo image_view_info = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .image    = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = create_info.format,
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

        VkImageView image_view = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateImageView(context->device(), &image_view_info, nullptr, &image_view));

        const VkSamplerCreateInfo sampler_info = {
            .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .magFilter               = create_info.mag_filter,
            .minFilter               = create_info.min_filter,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU            = create_info.address_mode_u,
            .addressModeV            = create_info.address_mode_v,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 0.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_ALWAYS,
            .minLod                  = 0.0f,
            .maxLod                  = 0.0f,
            .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkSampler sampler = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateSampler(context->device(), &sampler_info, nullptr, &sampler));

        VkDescriptorImageInfo descriptor_image_info = {
            .sampler     = sampler,
            .imageView   = image_view,
            .imageLayout = image_layout,
        };

        auto texture               = std::shared_ptr<Texture>(new Texture());
        texture->m_context         = context;
        texture->m_handle          = image;
        texture->m_image_view      = image_view;
        texture->m_sampler         = sampler;
        texture->m_format          = create_info.format;
        texture->m_descriptor      = descriptor_image_info;
        texture->m_allocation      = allocation;
        texture->m_allocation_info = allocation_info;
        texture->m_layout          = image_layout;
        texture->m_width           = width;
        texture->m_height          = height;
        texture->m_depth           = 1;
        texture->m_mip_levels      = 1;
        texture->m_layer_count     = 1;

        return texture;
    }

    std::shared_ptr<Texture> Texture::create(const std::shared_ptr<VulkanContext> &context,
                                             const TextureCreateInfo &create_info, uint32_t width, uint32_t height) {
        MILG_INFO("Creating texture {}x{} with format {}", width, height, string_VkFormat(create_info.format));

        VkImageUsageFlags       usage_flags       = create_info.usage;
        const VkImageCreateInfo image_create_info = {
            .sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext     = nullptr,
            .flags     = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format    = create_info.format,
            .extent =
                {
                    .width  = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth  = 1,
                },
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = usage_flags,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        const VmaAllocationCreateInfo allocation_create_info = {
            .flags          = 0,
            .usage          = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags  = 0,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = VK_NULL_HANDLE,
            .pUserData      = nullptr,
            .priority       = 0.0f,
        };

        VkImage           image           = VK_NULL_HANDLE;
        VmaAllocation     allocation      = VK_NULL_HANDLE;
        VmaAllocationInfo allocation_info = {};
        VK_CHECK(vmaCreateImage(context->allocator(), &image_create_info, &allocation_create_info, &image, &allocation,
                                &allocation_info));

        const VkImageViewCreateInfo image_view_info = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .image    = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = create_info.format,
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

        VkImageView image_view = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateImageView(context->device(), &image_view_info, nullptr, &image_view));

        const VkSamplerCreateInfo sampler_info = {
            .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .magFilter               = create_info.mag_filter,
            .minFilter               = create_info.min_filter,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU            = create_info.address_mode_u,
            .addressModeV            = create_info.address_mode_v,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 0.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_ALWAYS,
            .minLod                  = 0.0f,
            .maxLod                  = 0.0f,
            .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkSampler sampler = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateSampler(context->device(), &sampler_info, nullptr, &sampler));

        VkDescriptorImageInfo descriptor_image_info = {
            .sampler     = sampler,
            .imageView   = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        auto texture               = std::shared_ptr<Texture>(new Texture());
        texture->m_context         = context;
        texture->m_handle          = image;
        texture->m_image_view      = image_view;
        texture->m_sampler         = sampler;
        texture->m_format          = create_info.format;
        texture->m_descriptor      = descriptor_image_info;
        texture->m_allocation      = allocation;
        texture->m_allocation_info = allocation_info;
        texture->m_layout          = VK_IMAGE_LAYOUT_UNDEFINED;
        texture->m_width           = width;
        texture->m_height          = height;
        texture->m_depth           = 1;
        texture->m_mip_levels      = 1;
        texture->m_layer_count     = 1;

        return texture;
    }

    Texture::~Texture() {
        vmaDestroyImage(m_context->allocator(), m_handle, m_allocation);
        m_context->device_table().vkDestroySampler(m_context->device(), m_sampler, nullptr);
        m_context->device_table().vkDestroyImageView(m_context->device(), m_image_view, nullptr);
    }

    void Texture::transition_layout(VkCommandBuffer command_buffer, VkImageLayout new_layout) {
        m_context->transition_image_layout(command_buffer, m_handle, m_layout, new_layout);
        m_layout = new_layout;
    }

    VkImage Texture::handle() const {
        return m_handle;
    }

    VkImageView Texture::image_view() const {
        return m_image_view;
    }

    VkSampler Texture::sampler() const {
        return m_sampler;
    }

    VkFormat Texture::format() const {
        return m_format;
    }

    VkImageLayout Texture::layout() const {
        return m_layout;
    }

    VkDescriptorImageInfo Texture::descriptor() const {
        return m_descriptor;
    }

    VmaAllocation Texture::allocation() const {
        return m_allocation;
    }

    VmaAllocationInfo Texture::allocation_info() const {
        return m_allocation_info;
    }

    uint32_t Texture::width() const {
        return m_width;
    }

    uint32_t Texture::height() const {
        return m_height;
    }

    uint32_t Texture::depth() const {
        return m_depth;
    }

    uint32_t Texture::mip_levels() const {
        return m_mip_levels;
    }

    uint32_t Texture::layer_count() const {
        return m_layer_count;
    }
} // namespace milg
