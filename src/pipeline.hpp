#pragma once

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "texture.hpp"
#include "vk_context.hpp"

namespace milg {
    struct PipelineOutputDescription {
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width  = 0;
        uint32_t height = 0;
    };

    struct Pipeline {
        VkPipeline       pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout   = VK_NULL_HANDLE;

        VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
        VkDescriptorSet       set        = VK_NULL_HANDLE;

        std::vector<std::shared_ptr<Texture>> output_buffers;

        void bind_texture(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                          uint32_t binding, const std::shared_ptr<Texture> &texture);

        void bind_pipeline(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                           uint32_t push_constant_size = 0, const void *push_constant_data = nullptr);

        void set_push_constants(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                                uint32_t size, const void *data);
    };

    class PipelineFactory {
    public:
        static std::shared_ptr<PipelineFactory> create(const std::filesystem::path          &bindir,
                                                       const std::shared_ptr<VulkanContext> &context);

        ~PipelineFactory();

        Pipeline *create_compute_pipeline(const std::string &name, const std::filesystem::path &shader_path,
                                          const std::initializer_list<PipelineOutputDescription> &output_descriptions,
                                          uint32_t texture_input_count, uint32_t push_constant_size = 0);

        Pipeline *get_pipeline(const std::string &name);

    private:
        std::shared_ptr<VulkanContext> m_context = nullptr;

        std::filesystem::path                     m_bindir                 = "";
        VkDescriptorPool                          m_global_descriptor_pool = VK_NULL_HANDLE;
        std::unordered_map<std::string, Pipeline> m_pipelines;

        PipelineFactory() = default;
    };
} // namespace milg
