#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "application.hpp"
#include "audio.hpp"
#include "event.hpp"
#include "events.hpp"
#include "layer.hpp"
#include "logging.hpp"
#include "swapchain.hpp"
#include "vk_context.hpp"
#include "window.hpp"

using namespace milg;

static std::filesystem::path               bindir;
static std::map<std::string, audio::Sound> sounds;

struct Image {
    VkImage        handle = VK_NULL_HANDLE;
    VkImageView    view   = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkExtent3D     extent = {0, 0, 0};
    VkFormat       format = VK_FORMAT_UNDEFINED;
    VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;

    void transition_layout(const std::shared_ptr<VulkanContext> &context, VkCommandBuffer command_buffer,
                           VkImageLayout new_layout) {
        context->transition_image_layout(command_buffer, handle, format, layout, new_layout);
        layout = new_layout;
    }

    void destroy(const std::shared_ptr<VulkanContext> &context) {
        context->device_table().vkDestroyImageView(context->device(), view, nullptr);
        context->device_table().vkDestroyImage(context->device(), handle, nullptr);
        context->device_table().vkFreeMemory(context->device(), memory, nullptr);
    }
};

Image create_draw_image(const std::shared_ptr<VulkanContext> &context, uint32_t width, uint32_t height) {
    VkImageCreateInfo image_create_info = {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext       = nullptr,
        .flags       = 0,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_R8G8B8A8_UNORM,
        .extent      = {width, height, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage image_handle = VK_NULL_HANDLE;
    VK_CHECK(context->device_table().vkCreateImage(context->device(), &image_create_info, nullptr, &image_handle));

    VkMemoryRequirements memory_requirements;
    context->device_table().vkGetImageMemoryRequirements(context->device(), image_handle, &memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info = {
        .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext          = nullptr,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex =
            context->find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VkDeviceMemory memory_handle = VK_NULL_HANDLE;
    VK_CHECK(
        context->device_table().vkAllocateMemory(context->device(), &memory_allocate_info, nullptr, &memory_handle));
    VK_CHECK(context->device_table().vkBindImageMemory(context->device(), image_handle, memory_handle, 0));

    VkImageViewCreateInfo image_view_create_info = {.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                    .pNext    = nullptr,
                                                    .flags    = 0,
                                                    .image    = image_handle,
                                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                    .format   = VK_FORMAT_R8G8B8A8_UNORM,
                                                    .components =
                                                        {
                                                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        },
                                                    .subresourceRange = {.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                         .baseMipLevel   = 0,
                                                                         .levelCount     = 1,
                                                                         .baseArrayLayer = 0,
                                                                         .layerCount     = 1}};

    VkImageView image_view = VK_NULL_HANDLE;
    VK_CHECK(
        context->device_table().vkCreateImageView(context->device(), &image_view_create_info, nullptr, &image_view));

    return Image{
        .handle = image_handle,
        .view   = image_view,
        .memory = memory_handle,
        .extent = image_create_info.extent,
        .format = image_create_info.format,
        .layout = image_create_info.initialLayout,
    };
}

VkShaderModule load_shader_module(const std::filesystem::path &path, const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Loading shader module from file: {}", path.string());
    std::ifstream file(path);
    if (!file.is_open()) {
        file = std::ifstream(bindir / path);
    }
    if (!file.is_open()) {
        std::printf("Failed to open file\n");
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

    VkShaderModule shader_module;
    VK_CHECK(
        context->device_table().vkCreateShaderModule(context->device(), &shader_module_info, nullptr, &shader_module));

    return shader_module;
}

VkDescriptorSetLayout create_descriptor_set_layout(const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Creating descriptor set layout");
    const VkDescriptorSetLayoutBinding layout_binding = {
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };

    const VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = 1,
        .pBindings    = &layout_binding,
    };

    VkDescriptorSetLayout descriptor_set_layout;
    VK_CHECK(context->device_table().vkCreateDescriptorSetLayout(context->device(), &layout_info, nullptr,
                                                                 &descriptor_set_layout));

    return descriptor_set_layout;
}

VkPipelineLayout create_pipeline_layout(const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Creating pipeline layout");
    const VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(float) * 4,
    };

    const VkPipelineLayoutCreateInfo layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range,
    };

    VkPipelineLayout pipeline_layout;
    VK_CHECK(
        context->device_table().vkCreatePipelineLayout(context->device(), &layout_info, nullptr, &pipeline_layout));

    return pipeline_layout;
}

VkPipeline create_graphics_pipeline(VkShaderModule vertex_shader_module, VkShaderModule fragment_shader_module,
                                    VkPipelineLayout pipeline_layout, VkFormat surface_format,
                                    const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Creating graphics pipeline");
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
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment,
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

    const VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = vertex_shader_module,
            .pName               = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = fragment_shader_module,
            .pName               = "main",
            .pSpecializationInfo = nullptr,
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineRenderingCreateInfo dynamic_rendering_info = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .pNext                   = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &surface_format,
        .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &dynamic_rendering_info,
        .flags               = 0,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
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

    VkPipeline pipeline;
    VK_CHECK(context->device_table().vkCreateGraphicsPipelines(context->device(), VK_NULL_HANDLE, 1, &pipeline_info,
                                                               nullptr, &pipeline));

    return pipeline;
}

class Milg : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;

    VkShaderModule   vertex_shader_module   = VK_NULL_HANDLE;
    VkShaderModule   fragment_shader_module = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout        = VK_NULL_HANDLE;
    VkPipeline       pipeline               = VK_NULL_HANDLE;

    Image framebuffer = {};

    struct {
        float tint[3] = {1.0f, 1.0f, 1.0f};
        float size    = 1.0f;
    } push_constants;

    void on_attach() override {
        MILG_INFO("Starting Milg");
        context      = Application::get().context();
        auto &window = Application::get().window();

        this->vertex_shader_module   = load_shader_module("data/shaders/triangle.vert.spv", context);
        this->fragment_shader_module = load_shader_module("data/shaders/triangle.frag.spv", context);
        this->pipeline_layout        = create_pipeline_layout(context);

        this->framebuffer = create_draw_image(context, window->width(), window->height());
        this->pipeline    = create_graphics_pipeline(vertex_shader_module, fragment_shader_module, pipeline_layout,
                                                     framebuffer.format, context);
    }

    void on_update(float delta) override {
        ImGui::Begin("Tryngle");

        if (ImGui::BeginTabBar("milg")) {
            if (ImGui::BeginTabItem("Triangle")) {
                ImGui::ColorEdit3("Tint", push_constants.tint);
                ImGui::SliderFloat("Size", &push_constants.size, 0.1f, 1.0f);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio")) {
                auto master_volume = audio::get_volume();

                if (ImGui::SliderFloat("Master volume", &master_volume, 0.f, 1.f)) {
                    audio::set_volume(master_volume);
                }

                ImGui::SeparatorText("Loaded sounds");

                int        i              = 0;
                static int selected_index = 0;

                for (auto &[key, sound] : sounds) {
                    bool selected = selected_index == i;
                    auto volume   = sound.get_volume();

                    if (ImGui::Button(std::format("Play##{}", key).c_str())) {
                        sound.play();
                    }
                    ImGui::SameLine();
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderFloat(std::format("##vol_{}", key).c_str(), &volume, 0.f, 1.f)) {
                        sound.set_volume(volume);
                    }
                    ImGui::SameLine();
                    if (ImGui::Selectable(key.c_str(), selected)) {
                        selected_index = i;
                    }

                    i++;
                }

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();

        auto swapchain      = Application::get().swapchain();
        auto command_buffer = Application::get().aquire_command_buffer();

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };
        context->device_table().vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        framebuffer.transition_layout(context, command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        const VkExtent2D extent   = {framebuffer.extent.width, framebuffer.extent.height};
        const VkRect2D   scissor  = {{0, 0}, extent};
        const VkViewport viewport = {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = (float)extent.width,
            .height   = (float)extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRenderingAttachmentInfo rendering_attachment_info = {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = framebuffer.view,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
        };
        const VkRenderingInfo rendering_info = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {{0, 0}, extent},
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &rendering_attachment_info,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr,
        };
        context->device_table().vkCmdBeginRendering(command_buffer, &rendering_info);
        context->device_table().vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        context->device_table().vkCmdPushConstants(command_buffer, pipeline_layout,
                                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                   sizeof(push_constants), &push_constants);
        context->device_table().vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        context->device_table().vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        context->device_table().vkCmdDraw(command_buffer, 3, 1, 0, 0);
        context->device_table().vkCmdEndRendering(command_buffer);

        framebuffer.transition_layout(context, command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        Application::get().swapchain()->blit_to_current_image(command_buffer, framebuffer.handle, extent);
        context->device_table().vkEndCommandBuffer(command_buffer);
    }

    void on_event(Event &event) override {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<KeyPressedEvent>(BIND_EVENT_FN(Milg::on_key_pressed));
        dispatcher.dispatch<KeyReleasedEvent>(BIND_EVENT_FN(Milg::on_key_released));
    }

    bool on_key_pressed(KeyPressedEvent &event) {
        MILG_INFO("Key pressed: {}", event.key_code());

        return false;
    }

    bool on_key_released(KeyReleasedEvent &event) {
        MILG_INFO("Key released: {}", event.key_code());

        return false;
    }

    void on_detach() override {
        MILG_INFO("Shutting down Milg");

        framebuffer.destroy(context);
        vkDestroyShaderModule(context->device(), fragment_shader_module, nullptr);
        vkDestroyShaderModule(context->device(), vertex_shader_module, nullptr);
        vkDestroyPipelineLayout(context->device(), pipeline_layout, nullptr);
        vkDestroyPipeline(context->device(), pipeline, nullptr);
    }
};

class Milglication : public Application {
public:
    Milglication(const WindowCreateInfo &window_info) : Application(window_info) {
        push_layer(new Milg());
    }

    ~Milglication() {
    }
};

int main(int argc, char **argv) {
    bindir = std::filesystem::path(argv[0]).parent_path();
    Logging::init();

    WindowCreateInfo window_info = {
        .title  = "Milg",
        .width  = 800,
        .height = 600,
    };

    Milglication app(window_info);

    sounds.emplace("c1a0_sci_dis1d", "data/c1a0_sci_dis1d.wav");
    sounds.emplace("c1a0_sci_dis10a", "data/c1a0_sci_dis10a.wav");

    app.run();

    return 0;
}
