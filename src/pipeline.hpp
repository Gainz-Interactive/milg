#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <memory>
#include <milg.hpp>
#include <string>
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

        VkQueryPool query_pool = VK_NULL_HANDLE;

        uint32_t query_index    = 0;
        float    execution_time = 0;

        std::vector<std::shared_ptr<Texture>> output_buffers;

        void bind_texture(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                          uint32_t binding, const std::shared_ptr<Texture> &texture);

        void begin(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                   uint32_t push_constant_size = 0, const void *push_constant_data = nullptr);
        void end(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer);

        void set_push_constants(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                                uint32_t size, const void *data);
    };

    class PipelineFactory {
    public:
        static std::shared_ptr<PipelineFactory> create(const std::shared_ptr<VulkanContext> &context);

        ~PipelineFactory();

        Pipeline *create_compute_pipeline(const std::string &name, const std::string &shader_id,
                                          const std::initializer_list<PipelineOutputDescription> &output_descriptions,
                                          uint32_t texture_input_count, uint32_t push_constant_size = 0);
        void      begin_frame(VkCommandBuffer command_buffer);
        void      end_frame(VkCommandBuffer command_buffer);

        Pipeline *get_pipeline(const std::string &name);

        const std::map<std::string, Pipeline> &get_pipelines() const;

        float pre_execution_time() const;

    private:
        std::shared_ptr<VulkanContext> m_context = nullptr;

        VkDescriptorPool                m_global_descriptor_pool = VK_NULL_HANDLE;
        std::map<std::string, Pipeline> m_pipelines;
        std::array<VkQueryPool, 2>      m_query_pools = {VK_NULL_HANDLE, VK_NULL_HANDLE};

        float    m_pre_execution_time = 0;
        uint32_t m_frame_index        = true;

        PipelineFactory() = default;
    };
} // namespace milg
