#include <milg/graphics/sprite_batch.hpp>

#include <milg/core/asset.hpp>
#include <milg/core/logging.hpp>
#include <milg/graphics/vk_context.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace milg::graphics {
    VkShaderModule load_shader_module(const std::shared_ptr<Bytes>         &bytes,
                                      const std::shared_ptr<VulkanContext> &context) {

        const VkShaderModuleCreateInfo shader_module_info = {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = bytes->size(),
            .pCode    = reinterpret_cast<const uint32_t *>(bytes->data()),
        };

        VkShaderModule shader_module;
        VK_CHECK(context->device_table().vkCreateShaderModule(context->device(), &shader_module_info, nullptr,
                                                              &shader_module));

        return shader_module;
    }

    std::shared_ptr<SpriteBatch> SpriteBatch::create(const std::shared_ptr<VulkanContext> &context,
                                                     VkFormat albdedo_render_format, uint32_t capacity) {
        MILG_INFO("Creating sprite batch with capacity: {}", capacity);

        VkShaderModule vertex_shader_module   = VK_NULL_HANDLE;
        VkShaderModule fragment_shader_module = VK_NULL_HANDLE;

        if (auto shader = AssetStore::load<Bytes>("shaders/sprite_batch.vert.spv"); shader.has_value()) {
            vertex_shader_module = load_shader_module(*shader, context);
        } else {
            MILG_ERROR("Vertex shader not loaded");

            return nullptr;
        }

        if (auto shader = AssetStore::load<Bytes>("shaders/sprite_batch.frag.spv"); shader.has_value()) {
            fragment_shader_module = load_shader_module(*shader, context);
        } else {
            MILG_ERROR("Fragment shader not loaded");

            return nullptr;
        }

        if (vertex_shader_module == VK_NULL_HANDLE || fragment_shader_module == VK_NULL_HANDLE) {
            MILG_ERROR("Failed to load shader modules");
            return nullptr;
        }

        VkPhysicalDeviceType     device_type = context->device_properties().deviceType;
        VmaAllocationCreateFlags allocation_flags =
            device_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
                : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaMemoryUsage memory_usage = device_type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                                          ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                                          : VMA_MEMORY_USAGE_AUTO;

        VkBufferUsageFlags buffer_usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        BufferCreateInfo buffer_create_info = {
            .size             = capacity * Sprite::ATTRIB_COUNT * sizeof(float),
            .memory_usage     = memory_usage,
            .allocation_flags = allocation_flags,
            .usage_flags      = buffer_usage_flags,
        };
        auto geometry_buffer = Buffer::create(context, buffer_create_info);

        VkMemoryPropertyFlags memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        vmaGetMemoryTypeProperties(context->allocator(), geometry_buffer->allocation_info().memoryType,
                                   &memory_property_flags);

        std::shared_ptr<Buffer> backing_buffer = nullptr;
        if (memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
            !(memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            // If the buffer ended up in device local, non host visible memory, we
            // need to create a staging buffer that is host visible and mappable to
            // copy the data to the device local buffer later on
            MILG_INFO("Creating device local, non host visible buffer");
            buffer_create_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            buffer_create_info.usage_flags  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_create_info.allocation_flags =
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

            backing_buffer = Buffer::create(context, buffer_create_info);
        } else {
            // If the buffer ended up in host visible, mappable memory, we'll use a
            // vector to store the data and copy it to the buffer later on in
            // one go
            MILG_INFO("Creating host visible, mappable buffer");
        }

        const std::array<VkDescriptorPoolSize, 2> pool_sizes = {
            VkDescriptorPoolSize{
                .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = SpriteBatch::TEXTURE_DESCRIPTOR_BINDING_COUNT,
            },
            {
                .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
            },
        };

        const VkDescriptorPoolCreateInfo descriptor_pool_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets       = SpriteBatch::TEXTURE_DESCRIPTOR_BINDING_COUNT + 1,
            .poolSizeCount = pool_sizes.size(),
            .pPoolSizes    = pool_sizes.data(),
        };

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateDescriptorPool(context->device(), &descriptor_pool_info, nullptr,
                                                                &descriptor_pool));

        const std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
            VkDescriptorSetLayoutBinding{
                .binding            = 0,
                .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount    = 1,
                .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
            {
                .binding            = 1,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = SpriteBatch::TEXTURE_DESCRIPTOR_BINDING_COUNT,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            },
        };

        const std::array<VkDescriptorBindingFlags, 2> layout_flags = {
            0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                   VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

        const VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext         = nullptr,
            .bindingCount  = layout_flags.size(),
            .pBindingFlags = layout_flags.data(),
        };

        const VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = &binding_flags_info,
            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = bindings.size(),
            .pBindings    = bindings.data(),
        };

        VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateDescriptorSetLayout(context->device(), &layout_info, nullptr,
                                                                     &descriptor_set_layout));

        uint32_t descriptor_count = SpriteBatch::TEXTURE_DESCRIPTOR_BINDING_COUNT;
        const VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorSetCount = 1,
            .pDescriptorCounts  = &descriptor_count,
        };

        const VkDescriptorSetAllocateInfo descriptor_set_info = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = &variable_count_info,
            .descriptorPool     = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descriptor_set_layout,
        };

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        VK_CHECK(
            context->device_table().vkAllocateDescriptorSets(context->device(), &descriptor_set_info, &descriptor_set));

        const VkPushConstantRange push_constants = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset     = 0,
            .size       = sizeof(glm::mat4),
        };

        const VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 1,
            .pSetLayouts            = &descriptor_set_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &push_constants,
        };

        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreatePipelineLayout(context->device(), &pipeline_layout_info, nullptr,
                                                                &pipeline_layout));

        std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_VERTEX_BIT,
                .module              = vertex_shader_module,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module              = fragment_shader_module,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
        };

        const VkVertexInputBindingDescription vertex_binding = {
            .binding   = 0,
            .stride    = Sprite::ATTRIB_COUNT * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        const std::array<VkVertexInputAttributeDescription, 4> vertex_attribs = {
            // x, y, width, height
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset   = 0,
            },
            // u, v, u2, v2
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset   = 4 * sizeof(float),
            },
            // r, g, b, a
            VkVertexInputAttributeDescription{
                .location = 2,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset   = 8 * sizeof(float),
            },
            // rotation, texture_index
            VkVertexInputAttributeDescription{
                .location = 3,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = 12 * sizeof(float),
            },
        };

        const VkPipelineVertexInputStateCreateInfo vertex_input = {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &vertex_binding,
            .vertexAttributeDescriptionCount = vertex_attribs.size(),
            .pVertexAttributeDescriptions    = vertex_attribs.data(),
        };

        const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        const VkPipelineRasterizationStateCreateInfo rasterizer_info = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp          = 0.0f,
            .depthBiasSlopeFactor    = 0.0f,
            .lineWidth               = 1.0f,
        };

        const VkPipelineMultisampleStateCreateInfo multisample_info = {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 0.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        };

        const VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable         = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };

        std::array<VkPipelineColorBlendAttachmentState, 1> color_blend_attachments = {
            color_blend_attachment,
        };

        const VkPipelineColorBlendStateCreateInfo color_blend_info = {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<uint32_t>(color_blend_attachments.size()),
            .pAttachments    = color_blend_attachments.data(),
            .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
        };

        const std::array<VkDynamicState, 2> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        const VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext             = nullptr,
            .flags             = 0,
            .dynamicStateCount = dynamic_states.size(),
            .pDynamicStates    = dynamic_states.data(),
        };

        const VkPipelineViewportStateCreateInfo viewport_state = {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = nullptr,
            .scissorCount  = 1,
            .pScissors     = nullptr,
        };

        const std::array<VkFormat, 1> render_formats = {albdedo_render_format};

        const VkPipelineRenderingCreateInfo dynamic_rendering_info = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
            .pNext                   = nullptr,
            .colorAttachmentCount    = static_cast<uint32_t>(render_formats.size()),
            .pColorAttachmentFormats = render_formats.data(),
            .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
        };

        const VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = &dynamic_rendering_info,
            .flags               = 0,
            .stageCount          = shader_stages.size(),
            .pStages             = shader_stages.data(),
            .pVertexInputState   = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewport_state,
            .pRasterizationState = &rasterizer_info,
            .pMultisampleState   = &multisample_info,
            .pDepthStencilState  = nullptr,
            .pColorBlendState    = &color_blend_info,
            .pDynamicState       = &dynamic_state,
            .layout              = pipeline_layout,
            .renderPass          = VK_NULL_HANDLE,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        VK_CHECK(context->device_table().vkCreateGraphicsPipelines(context->device(), VK_NULL_HANDLE, 1, &pipeline_info,
                                                                   nullptr, &pipeline));

        auto batch                      = std::shared_ptr<SpriteBatch>(new SpriteBatch());
        batch->m_context                = context;
        batch->m_capacity               = capacity;
        batch->m_geometry_buffer        = geometry_buffer;
        batch->m_backing_buffer         = backing_buffer;
        batch->m_descriptor_pool        = descriptor_pool;
        batch->m_descriptor_set_layout  = descriptor_set_layout;
        batch->m_descriptor_set         = descriptor_set;
        batch->m_pipeline_layout        = pipeline_layout;
        batch->m_pipeline               = pipeline;
        batch->m_vertex_shader_module   = vertex_shader_module;
        batch->m_fragment_shader_module = fragment_shader_module;

        if (backing_buffer == nullptr) {
            batch->m_geometry_cache.resize(Sprite::ATTRIB_COUNT * capacity);
        }

        return batch;
    }

    void SpriteBatch::draw_sprite(Sprite &sprite, const std::shared_ptr<Texture> &texture) {
        if (m_sprite_count >= m_capacity) {
            MILG_ERROR("SpriteBatch::draw_sprite: Exceeded capacity");
            return;
        }

        if (m_batches.size() == 0) {
            MILG_ERROR("SpriteBatch::draw_sprite: No active batch");
            return;
        }

        auto &batch = m_batches.back();

        sprite.texture_index = register_texture(texture);

        float *geometry_data = this->m_backing_buffer
                                   ? reinterpret_cast<float *>(this->m_backing_buffer->allocation_info().pMappedData)
                                   : this->m_geometry_cache.data();

        const size_t offset = m_sprite_count * Sprite::ATTRIB_COUNT;
        memcpy(&geometry_data[offset], &sprite, Sprite::ATTRIB_COUNT * sizeof(float));
        m_sprite_count++;
        batch.count++;
    }

    void SpriteBatch::reset() {
        m_texture_indices.clear();
        m_batches.clear();

        m_sprite_count = 0;
    }

    void SpriteBatch::begin_batch(const glm::mat4 &matrix) {
        uint32_t start_index = m_batches.empty() ? 0 : m_batches.back().start_index + m_batches.back().count;

        m_batches.push_back({
            .start_index   = start_index,
            .count         = 0,
            .constant_data = {matrix},
        });
    }

    void SpriteBatch::build_batches(VkCommandBuffer command_buffer) {
        if (m_batches.empty() || m_sprite_count == 0) {
            return;
        }

        if (this->m_backing_buffer) {
            const VkBufferCopy copy_region = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = m_sprite_count * Sprite::ATTRIB_COUNT * sizeof(float),
            };

            m_context->device_table().vkCmdCopyBuffer(command_buffer, this->m_backing_buffer->handle(),
                                                      this->m_geometry_buffer->handle(), 1, &copy_region);
        } else {
            memcpy(this->m_geometry_buffer->allocation_info().pMappedData, this->m_geometry_cache.data(),
                   m_sprite_count * Sprite::ATTRIB_COUNT * sizeof(float));
        }
    }

    void SpriteBatch::render(VkCommandBuffer command_buffer) {
        {
            std::vector<VkWriteDescriptorSet> write_sets;
            for (const auto &[texture, descriptor] : this->m_texture_indices) {
                const VkWriteDescriptorSet descriptor_write = {
                    .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext            = nullptr,
                    .dstSet           = m_descriptor_set,
                    .dstBinding       = 1,
                    .dstArrayElement  = descriptor.index,
                    .descriptorCount  = 1,
                    .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo       = &descriptor.image_info,
                    .pBufferInfo      = nullptr,
                    .pTexelBufferView = nullptr,
                };

                write_sets.push_back(descriptor_write);
            }
            m_context->device_table().vkUpdateDescriptorSets(m_context->device(), write_sets.size(), write_sets.data(),
                                                             0, nullptr);
        }

        m_context->device_table().vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        m_context->device_table().vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                          m_pipeline_layout, 0, 1, &m_descriptor_set, 0, nullptr);

        size_t   offset           = 0;
        VkBuffer vertex_buffers[] = {m_geometry_buffer->handle()};
        m_context->device_table().vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffers[0], &offset);

        for (const auto &batch : this->m_batches) {
            m_context->device_table().vkCmdPushConstants(command_buffer, m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
                                                         0, sizeof(BatchConstantData), &batch.constant_data);
            m_context->device_table().vkCmdDraw(command_buffer, 6, batch.count, batch.start_index, 0);
        }
    }

    uint32_t SpriteBatch::register_texture(const std::shared_ptr<Texture> &texture) {
        if (m_texture_indices.find(texture) != m_texture_indices.end()) {
            return m_texture_indices[texture].index;
        }

        uint32_t index             = m_texture_indices.size();
        m_texture_indices[texture] = {
            .index = index,
            .image_info =
                {
                    .sampler     = texture->sampler(),
                    .imageView   = texture->image_view(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
        };

        return index;
    }

    uint32_t SpriteBatch::capacity() const {
        return m_capacity;
    }

    uint32_t SpriteBatch::sprite_count() const {
        return m_sprite_count;
    }

    uint32_t SpriteBatch::batch_count() const {
        return m_batches.size();
    }

    uint32_t SpriteBatch::texture_count() const {
        return m_texture_indices.size();
    }

    SpriteBatch::~SpriteBatch() {
        m_context->device_table().vkDestroyPipeline(m_context->device(), m_pipeline, nullptr);
        m_context->device_table().vkDestroyPipelineLayout(m_context->device(), m_pipeline_layout, nullptr);
        m_context->device_table().vkDestroyDescriptorSetLayout(m_context->device(), m_descriptor_set_layout, nullptr);
        m_context->device_table().vkDestroyDescriptorPool(m_context->device(), m_descriptor_pool, nullptr);
        m_context->device_table().vkDestroyShaderModule(m_context->device(), m_vertex_shader_module, nullptr);
        m_context->device_table().vkDestroyShaderModule(m_context->device(), m_fragment_shader_module, nullptr);
    }
} // namespace milg::graphics
