#include "pipeline.hpp"

#include "logging.hpp"

#include <array>
#include <fstream>

namespace milg {
    std::shared_ptr<PipelineFactory> PipelineFactory::create(const std::filesystem::path          &bindir,
                                                             const std::shared_ptr<VulkanContext> &context) {
        std::array<VkDescriptorPoolSize, 4> poolSizes = {
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        };

        const VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = 1000,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes    = poolSizes.data(),
        };

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        context->device_table().vkCreateDescriptorPool(context->device(), &poolInfo, nullptr, &descriptorPool);

        auto factory                      = std::shared_ptr<PipelineFactory>(new PipelineFactory());
        factory->m_context                = context;
        factory->m_global_descriptor_pool = descriptorPool;
        factory->m_bindir                 = bindir;

        return factory;
    }

    PipelineFactory::~PipelineFactory() {
        for (auto &pipeline : m_pipelines) {
            m_context->device_table().vkDestroyPipeline(m_context->device(), pipeline.second.pipeline, nullptr);
            m_context->device_table().vkDestroyPipelineLayout(m_context->device(), pipeline.second.layout, nullptr);
            m_context->device_table().vkDestroyDescriptorSetLayout(m_context->device(), pipeline.second.set_layout,
                                                                   nullptr);
        }
        m_context->device_table().vkDestroyDescriptorPool(m_context->device(), m_global_descriptor_pool, nullptr);

        m_pipelines.clear();
    }

    Pipeline *PipelineFactory::create_compute_pipeline(
        const std::string &name, const std::filesystem::path &shader_path,
        const std::initializer_list<PipelineOutputDescription> &output_descriptions, uint32_t texture_input_count,
        uint32_t push_constant_size) {
        if (m_pipelines.find(name) != m_pipelines.end()) {
            MILG_ERROR("Pipeline with name {} already exists", name);
            return nullptr;
        }

        std::vector<VkDescriptorSetLayoutBinding> texture_bindings;
        for (uint32_t i = 0; i < texture_input_count; i++) {
            texture_bindings.push_back({
                .binding            = i,
                .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount    = 1,
                .stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT,
                .pImmutableSamplers = nullptr,
            });
        }

        const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = nullptr,
            .flags        = 0,
            .bindingCount = static_cast<uint32_t>(texture_bindings.size()),
            .pBindings    = texture_bindings.data(),
        };

        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        m_context->device_table().vkCreateDescriptorSetLayout(m_context->device(), &descriptor_set_layout_info, nullptr,
                                                              &descriptor_set_layout);

        const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = m_global_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descriptor_set_layout,
        };

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        m_context->device_table().vkAllocateDescriptorSets(m_context->device(), &descriptor_set_allocate_info,
                                                           &descriptor_set);

        auto load_shader_module = [&](const std::filesystem::path &path) -> VkShaderModule {
            MILG_INFO("Loading shader module from file: {}", path.string());
            std::ifstream file(path);
            if (!file.is_open()) {
                file = std::ifstream(m_bindir / path);
            }
            if (!file.is_open()) {
                MILG_ERROR("Failed to open file: {}", path.string());
                return VK_NULL_HANDLE;
            }

            std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            const VkShaderModuleCreateInfo shader_module_info = {
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .codeSize = buffer.size(),
                .pCode    = reinterpret_cast<const uint32_t *>(buffer.data()),
            };

            VkShaderModule shader_module = VK_NULL_HANDLE;
            VK_CHECK(m_context->device_table().vkCreateShaderModule(m_context->device(), &shader_module_info, nullptr,
                                                                    &shader_module));

            return shader_module;
        };

        VkShaderModule shader_module = load_shader_module(shader_path);

        const VkPipelineShaderStageCreateInfo shader_stage_info = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_module,
            .pName  = "main",
        };

        const VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset     = 0,
            .size       = push_constant_size,
        };

        const VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 1,
            .pSetLayouts            = &descriptor_set_layout,
            .pushConstantRangeCount = push_constant_size > 0 ? 1u : 0u,
            .pPushConstantRanges    = push_constant_size > 0 ? &push_constant_range : nullptr,
        };

        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VK_CHECK(m_context->device_table().vkCreatePipelineLayout(m_context->device(), &pipeline_layout_info, nullptr,
                                                                  &pipeline_layout));

        const VkComputePipelineCreateInfo pipeline_info = {
            .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
            .stage              = shader_stage_info,
            .layout             = pipeline_layout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex  = 0,
        };

        VkPipeline pipeline_handle = VK_NULL_HANDLE;
        VK_CHECK(m_context->device_table().vkCreateComputePipelines(m_context->device(), VK_NULL_HANDLE, 1,
                                                                    &pipeline_info, nullptr, &pipeline_handle));

        m_pipelines[name] = {
            .pipeline   = pipeline_handle,
            .layout     = pipeline_layout,
            .set_layout = descriptor_set_layout,
            .set        = descriptor_set,
        };
        for (const auto &output_description : output_descriptions) {
            m_pipelines[name].output_buffers.push_back(
                Texture::create(m_context,
                                {
                                    .format = output_description.format,
                                    .usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                },
                                output_description.width, output_description.height));
        }
        vkDestroyShaderModule(m_context->device(), shader_module, nullptr);

        return &m_pipelines[name];
    }

    void Pipeline::bind_texture(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                                uint32_t binding, const std::shared_ptr<Texture> &texture) {
        VkDescriptorImageInfo image_info = {
            .sampler     = VK_NULL_HANDLE,
            .imageView   = texture->image_view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet write_descriptor_set = {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = set,
            .dstBinding       = binding,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo       = &image_info,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        };
        context->device_table().vkUpdateDescriptorSets(context->device(), 1, &write_descriptor_set, 0, nullptr);
    }

    void Pipeline::bind_pipeline(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                                 uint32_t push_constant_size, const void *push_constant_data) {
        context->device_table().vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context->device_table().vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                                                        &set, 0, nullptr);
        if (push_constant_size > 0) {
            set_push_constants(context, command_buffer, push_constant_size, push_constant_data);
        }
    }

    void Pipeline::set_push_constants(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                                      uint32_t size, const void *data) {
        context->device_table().vkCmdPushConstants(command_buffer, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, data);
    }
} // namespace milg
