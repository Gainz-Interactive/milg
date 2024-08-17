#include "imgui_layer.hpp"
#include "graphics/swapchain.hpp"
#include "imgui.h"
#include "window.hpp"
#include <memory>
#include <milg.hpp>

namespace milg {
    std::shared_ptr<ImGuiLayer> ImGuiLayer::create(const std::shared_ptr<graphics::Swapchain>     &swapchain,
                                                   const std::unique_ptr<Window>                  &window,
                                                   const std::shared_ptr<graphics::VulkanContext> &context) {

        MILG_INFO("Initializing ImGui context");
        ImGui::CreateContext();
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
        VK_CHECK(context->device_table().vkCreateDescriptorPool(context->device(), &pool_info, nullptr,
                                                                &imgui_descriptor_pool));

        auto layer               = std::shared_ptr<ImGuiLayer>(new ImGuiLayer());
        layer->m_descriptor_pool = imgui_descriptor_pool;
        layer->m_context         = context;
        layer->m_color_format    = swapchain->surface_format().format;

        ImGui_ImplVulkan_InitInfo imgui_init_info = {
            .Instance            = context->instance(),
            .PhysicalDevice      = context->physical_device(),
            .Device              = context->device(),
            .QueueFamily         = context->graphics_queue_family_index(),
            .Queue               = context->graphics_queue(),
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
                    .pColorAttachmentFormats = &layer->m_color_format,
                    .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
                    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
                },
        };
        ImGui_ImplVulkan_Init(&imgui_init_info);
        ImGui_ImplVulkan_CreateFontsTexture();

        return layer;
    }

    ImGuiLayer::~ImGuiLayer() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        m_context->device_table().vkDestroyDescriptorPool(m_context->device(), m_descriptor_pool, nullptr);
        ImGui::DestroyContext();
    }

    void ImGuiLayer::process_event(void *event) {
        ImGui_ImplSDL2_ProcessEvent((SDL_Event *)event);
    }
} // namespace milg
