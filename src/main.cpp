#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

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

std::tuple<VkCommandPool, std::vector<VkCommandBuffer>>
create_command_structures(const std::shared_ptr<VulkanContext> &context, size_t command_buffer_count) {
    MILG_INFO("Creating command structures");
    const VkCommandPoolCreateInfo command_pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = 0,
    };

    VkCommandPool command_pool;
    VK_CHECK(
        context->device_table().vkCreateCommandPool(context->device(), &command_pool_info, nullptr, &command_pool));

    const VkCommandBufferAllocateInfo command_buffer_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(command_buffer_count),
    };

    std::vector<VkCommandBuffer> command_buffers(command_buffer_count);
    VK_CHECK(context->device_table().vkAllocateCommandBuffers(context->device(), &command_buffer_info,
                                                              command_buffers.data()));

    return {command_pool, command_buffers};
}

std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>>
create_semaphores(const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Creating semaphores");
    std::vector<VkSemaphore> image_available_semaphores(2);
    std::vector<VkSemaphore> render_finished_semaphores(2);

    for (size_t i = 0; i < 2; ++i) {
        const VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };

        VK_CHECK(context->device_table().vkCreateSemaphore(context->device(), &semaphore_info, nullptr,
                                                           &image_available_semaphores[i]));
        VK_CHECK(context->device_table().vkCreateSemaphore(context->device(), &semaphore_info, nullptr,
                                                           &render_finished_semaphores[i]));
    }

    return {image_available_semaphores, render_finished_semaphores};
}

std::vector<VkFence> create_frame_fences(const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Creating frame fences");
    std::vector<VkFence> frame_fences(2);
    for (size_t i = 0; i < 2; ++i) {
        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VK_CHECK(context->device_table().vkCreateFence(context->device(), &fence_info, nullptr, &frame_fences[i]));
    }

    return frame_fences;
}

void transition_image(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkCommandBuffer command_buffer,
                      const std::shared_ptr<VulkanContext> &context) {
    const VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
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

    const VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    const VkPipelineStageFlags target_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    context->device_table().vkCmdPipelineBarrier(command_buffer, source_stage, target_stage, 0, 0, nullptr, 0, nullptr,
                                                 1, &barrier);
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

VkDescriptorPool initialize_imgui_context(const std::shared_ptr<Swapchain>     &swapchain,
                                          const std::unique_ptr<Window>        &window,
                                          const std::shared_ptr<VulkanContext> &context, VkQueue graphics_queue) {
    MILG_INFO("Initializing ImGui context");
    ImGuiContext *imgui_context = ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplSDL2_InitForVulkan(window->handle());

    const VkDescriptorPoolSize imgui_pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    const VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1000,
        .poolSizeCount = (uint32_t)std::size(imgui_pool_sizes),
        .pPoolSizes    = imgui_pool_sizes,
    };

    VkDescriptorPool imgui_descriptor_pool;
    VK_CHECK(
        context->device_table().vkCreateDescriptorPool(context->device(), &pool_info, nullptr, &imgui_descriptor_pool));

    VkFormat surface_format = swapchain->surface_format().format;

    ImGui_ImplVulkan_InitInfo imgui_init_info = {
        .Instance            = context->instance(),
        .PhysicalDevice      = context->physical_device(),
        .Device              = context->device(),
        .QueueFamily         = context->graphics_queue_family_index(),
        .Queue               = graphics_queue,
        .DescriptorPool      = imgui_descriptor_pool,
        .MinImageCount       = 2,
        .ImageCount          = 2,
        .MSAASamples         = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache       = VK_NULL_HANDLE,
        .Subpass             = 0,
        .UseDynamicRendering = VK_TRUE,
        .PipelineRenderingCreateInfo =
            {
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .pNext                   = nullptr,
                .viewMask                = 0,
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &surface_format,
                .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
                .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
            },
    };
    ImGui_ImplVulkan_Init(&imgui_init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    return imgui_descriptor_pool;
}

class Milg : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;

    VkQueue                      graphics_queue = VK_NULL_HANDLE;
    VkCommandPool                command_pool   = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkFence>         frame_fences;
    std::vector<VkSemaphore>     image_available_semaphores;
    std::vector<VkSemaphore>     render_finished_semaphores;
    VkShaderModule               vertex_shader_module   = VK_NULL_HANDLE;
    VkShaderModule               fragment_shader_module = VK_NULL_HANDLE;
    VkPipelineLayout             pipeline_layout        = VK_NULL_HANDLE;
    VkPipeline                   pipeline               = VK_NULL_HANDLE;
    VkDescriptorPool             imgui_descriptor_pool  = VK_NULL_HANDLE;

    uint32_t frame_index = 0;

    struct {
        float tint[3] = {1.0f, 1.0f, 1.0f};
        float size    = 1.0f;
    } push_constants;

    void on_attach() override {
        MILG_INFO("Starting Milg");
        context         = Application::get().context();
        auto &swapchain = Application::get().swapchain();

        context->device_table().vkGetDeviceQueue(context->device(), context->graphics_queue_family_index(), 0,
                                                 &this->graphics_queue);

        auto [command_pool, command_buffers] = create_command_structures(context, swapchain->image_count());
        this->command_pool                   = command_pool;
        this->command_buffers                = command_buffers;

        auto [image_available_semaphores, render_finished_semaphores] = create_semaphores(context);
        this->image_available_semaphores                              = image_available_semaphores;
        this->render_finished_semaphores                              = render_finished_semaphores;

        this->frame_fences = create_frame_fences(context);

        this->vertex_shader_module   = load_shader_module("data/shaders/triangle.vert.spv", context);
        this->fragment_shader_module = load_shader_module("data/shaders/triangle.frag.spv", context);
        this->pipeline_layout        = create_pipeline_layout(context);
        this->pipeline = create_graphics_pipeline(vertex_shader_module, fragment_shader_module, pipeline_layout,
                                                  swapchain->surface_format().format, context);
        this->imgui_descriptor_pool =
            initialize_imgui_context(swapchain, Application::get().window(), context, graphics_queue);
    }

    void on_detach() override {
        MILG_INFO("Shutting down Milg");

        context->device_table().vkDeviceWaitIdle(context->device());

        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(context->device(), imgui_descriptor_pool, nullptr);
        vkDestroyShaderModule(context->device(), fragment_shader_module, nullptr);
        vkDestroyShaderModule(context->device(), vertex_shader_module, nullptr);
        vkDestroyPipelineLayout(context->device(), pipeline_layout, nullptr);
        vkDestroyPipeline(context->device(), pipeline, nullptr);
        for (auto fence : frame_fences) {
            vkDestroyFence(context->device(), fence, nullptr);
        }
        for (auto semaphore : render_finished_semaphores) {
            vkDestroySemaphore(context->device(), semaphore, nullptr);
        }
        for (auto semaphore : image_available_semaphores) {
            vkDestroySemaphore(context->device(), semaphore, nullptr);
        }
        vkDestroyCommandPool(context->device(), command_pool, nullptr);
    }

    void on_update(float delta) override {
        auto &swapchain = Application::get().swapchain();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Tryngle");

        if (ImGui::BeginTabBar("milg")) {
            if (ImGui::BeginTabItem("Triangle")) {
                ImGui::ColorEdit3("Tint", push_constants.tint);
                ImGui::SliderFloat("Size", &push_constants.size, 0.1f, 1.0f);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio")) {
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
        ImGui::Render();

        context->device_table().vkWaitForFences(context->device(), 1, &frame_fences[frame_index], VK_TRUE, UINT64_MAX);
        context->device_table().vkResetFences(context->device(), 1, &frame_fences[frame_index]);

        uint32_t image_index;
        context->device_table().vkAcquireNextImageKHR(context->device(), swapchain->handle(), UINT64_MAX,
                                                      image_available_semaphores[frame_index], VK_NULL_HANDLE,
                                                      &image_index);

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        context->device_table().vkBeginCommandBuffer(command_buffers[image_index], &command_buffer_begin_info);

        transition_image(swapchain->get_image(image_index).image, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, command_buffers[image_index], context);

        const VkRenderingAttachmentInfo rendering_attachment_info = {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = swapchain->get_image(image_index).view,
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
            .renderArea           = {{0, 0}, swapchain->extent()},
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &rendering_attachment_info,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr,
        };
        context->device_table().vkCmdBeginRendering(command_buffers[image_index], &rendering_info);

        const VkRect2D   scissor  = {{0, 0}, {800, 600}};
        const VkViewport viewport = {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = 800.0f,
            .height   = 600.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        context->device_table().vkCmdBindPipeline(command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                  pipeline);

        context->device_table().vkCmdPushConstants(command_buffers[image_index], pipeline_layout,
                                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                                   sizeof(push_constants), &push_constants);
        context->device_table().vkCmdSetViewport(command_buffers[image_index], 0, 1, &viewport);
        context->device_table().vkCmdSetScissor(command_buffers[image_index], 0, 1, &scissor);

        context->device_table().vkCmdDraw(command_buffers[image_index], 3, 1, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffers[image_index]);

        context->device_table().vkCmdEndRendering(command_buffers[image_index]);

        transition_image(swapchain->get_image(image_index).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, command_buffers[image_index], context);

        context->device_table().vkEndCommandBuffer(command_buffers[image_index]);

        const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo         submit_info         = {
                            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .pNext                = nullptr,
                            .waitSemaphoreCount   = 1,
                            .pWaitSemaphores      = &image_available_semaphores[frame_index],
                            .pWaitDstStageMask    = &wait_dst_stage_mask,
                            .commandBufferCount   = 1,
                            .pCommandBuffers      = &command_buffers[image_index],
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores    = &render_finished_semaphores[frame_index],
        };

        context->device_table().vkQueueSubmit(graphics_queue, 1, &submit_info, frame_fences[frame_index]);

        VkSwapchainKHR         handle       = swapchain->handle();
        const VkPresentInfoKHR present_info = {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &render_finished_semaphores[frame_index],
            .swapchainCount     = 1,
            .pSwapchains        = &handle,
            .pImageIndices      = &image_index,
            .pResults           = nullptr,
        };

        context->device_table().vkQueuePresentKHR(graphics_queue, &present_info);

        frame_index = (frame_index + 1) % 2;
    }

    void on_event(Event &event) override {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<KeyPressedEvent>(BIND_EVENT_FN(Milg::on_key_pressed));
        dispatcher.dispatch<KeyReleasedEvent>(BIND_EVENT_FN(Milg::on_key_released));
        dispatcher.dispatch<RawEvent>(BIND_EVENT_FN(Milg::on_raw_event));
    }

    bool on_key_pressed(KeyPressedEvent &event) {
        MILG_INFO("Key pressed: {}", event.key_code());

        return false;
    }

    bool on_key_released(KeyReleasedEvent &event) {
        MILG_INFO("Key released: {}", event.key_code());
        if (auto sound = sounds.find("garsas"); sound != sounds.end()) {
            sound->second.play();
        }

        return false;
    }

    bool on_raw_event(RawEvent &event) {
        ImGui_ImplSDL2_ProcessEvent((SDL_Event *)event.raw_event());

        return false;
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

    sounds.emplace("garsas", "data/c1a0_sci_dis10a.wav");

    app.run();

    return 0;
}
