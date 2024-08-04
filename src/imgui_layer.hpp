#pragma once

#include "vk_context.hpp"

#include <memory>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace milg {
    class ImGuiLayer {
    public:
        static std::shared_ptr<ImGuiLayer> create(const std::shared_ptr<class Swapchain> &swapchain,
                                                  const std::unique_ptr<class Window>    &window,
                                                  const std::shared_ptr<VulkanContext>   &context);

        ~ImGuiLayer();

        void process_event(void *event);

    private:
        VkFormat                       m_color_format    = VK_FORMAT_UNDEFINED;
        VkDescriptorPool               m_descriptor_pool = VK_NULL_HANDLE;
        std::shared_ptr<VulkanContext> m_context         = nullptr;

        ImGuiLayer() = default;
    };
} // namespace milg
