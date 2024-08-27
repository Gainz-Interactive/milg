#pragma once

#include <milg/core/asset.hpp>
#include <milg/graphics/vk_context.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace milg::graphics {
    struct TextureCreateInfo {
        VkFormat          format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage  = VK_IMAGE_USAGE_SAMPLED_BIT;

        VkFilter min_filter = VK_FILTER_LINEAR;
        VkFilter mag_filter = VK_FILTER_LINEAR;

        VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };

    class Texture {
    public:
        class Loader : public milg::Asset::Loader {
        public:
            Loader() = delete;
            Loader(std::weak_ptr<VulkanContext> ctx);
            Loader(const Loader &) = default;
            Loader(Loader &&)      = default;

            Loader &operator=(const Loader &) = default;
            Loader &operator=(Loader &&)      = default;

            ~Loader() = default;

            std::shared_ptr<void> load(std::ifstream &stream);

        private:
            std::weak_ptr<VulkanContext> ctx;
        };

        static std::shared_ptr<Texture> load_from_data(const std::shared_ptr<VulkanContext> &context,
                                                       const TextureCreateInfo &create_info, const Bytes &bytes);

        static std::shared_ptr<Texture> create(const std::shared_ptr<VulkanContext> &context,
                                               const TextureCreateInfo &create_info, uint32_t width, uint32_t height);

        ~Texture();

        void transition_layout(VkCommandBuffer command_buffer, VkImageLayout new_layout);
        void blit_from(const std::shared_ptr<Texture> &src, VkCommandBuffer command_buffer);

        VkImage               handle() const;
        VkImageView           image_view() const;
        VkSampler             sampler() const;
        VkFormat              format() const;
        VkDescriptorImageInfo descriptor() const;
        VmaAllocation         allocation() const;
        VmaAllocationInfo     allocation_info() const;
        VkImageLayout         layout() const;

        uint32_t width() const;
        uint32_t height() const;
        uint32_t depth() const;
        uint32_t mip_levels() const;
        uint32_t layer_count() const;

    private:
        std::shared_ptr<VulkanContext> m_context = nullptr;

        VkImage               m_handle          = VK_NULL_HANDLE;
        VkImageView           m_image_view      = VK_NULL_HANDLE;
        VkSampler             m_sampler         = VK_NULL_HANDLE;
        VkFormat              m_format          = VK_FORMAT_UNDEFINED;
        VkDescriptorImageInfo m_descriptor      = {};
        VmaAllocation         m_allocation      = VK_NULL_HANDLE;
        VmaAllocationInfo     m_allocation_info = {};
        VkImageLayout         m_layout          = VK_IMAGE_LAYOUT_UNDEFINED;

        uint32_t m_width       = 0;
        uint32_t m_height      = 0;
        uint32_t m_depth       = 0;
        uint32_t m_mip_levels  = 0;
        uint32_t m_layer_count = 0;

        Texture() = default;
    };

} // namespace milg::graphics
