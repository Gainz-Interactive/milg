#pragma once

#include <milg/graphics/buffer.hpp>
#include <milg/graphics/sprite.hpp>
#include <milg/graphics/texture.hpp>
#include <milg/graphics/vk_context.hpp>

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace milg::graphics {
    class SpriteBatch {
    public:
        constexpr static uint32_t TEXTURE_DESCRIPTOR_BINDING_COUNT = 1024;

        static std::shared_ptr<SpriteBatch> create(const std::shared_ptr<VulkanContext> &context,
                                                   VkFormat albdedo_render_format, uint32_t capacity);

        ~SpriteBatch();

        void draw_sprite(Sprite &sprite, const std::shared_ptr<Texture> &texture);
        void reset();
        void begin_batch(const glm::mat4 &matrix);
        void build_batches(VkCommandBuffer command_buffer);
        void render(VkCommandBuffer command_buffer);

        uint32_t capacity() const;
        uint32_t sprite_count() const;
        uint32_t batch_count() const;
        uint32_t texture_count() const;

    private:
        struct TextureEntry {
            uint32_t              index      = 0;
            VkDescriptorImageInfo image_info = {};
        };

        struct BatchConstantData {
            glm::mat4 combined_matrix = glm::mat4(1.0f);
        };

        struct Batch {
            uint32_t          start_index   = 0;
            uint32_t          count         = 0;
            BatchConstantData constant_data = {};
        };

        std::shared_ptr<VulkanContext> m_context = nullptr;

        uint32_t                m_capacity        = 0;
        std::shared_ptr<Buffer> m_geometry_buffer = nullptr;
        std::shared_ptr<Buffer> m_backing_buffer  = nullptr;
        std::vector<float>      m_geometry_cache;

        VkDescriptorPool      m_descriptor_pool       = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSet       m_descriptor_set        = VK_NULL_HANDLE;

        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline       m_pipeline        = VK_NULL_HANDLE;

        VkShaderModule m_vertex_shader_module   = VK_NULL_HANDLE;
        VkShaderModule m_fragment_shader_module = VK_NULL_HANDLE;

        std::unordered_map<std::shared_ptr<Texture>, TextureEntry> m_texture_indices;

        uint32_t           m_sprite_count = 0;
        std::vector<Batch> m_batches;

        uint32_t register_texture(const std::shared_ptr<Texture> &texture);

        SpriteBatch() = default;
    };
} // namespace milg::graphics
