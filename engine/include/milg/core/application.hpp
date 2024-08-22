#pragma once

#include <milg/core/imgui_layer.hpp>
#include <milg/core/window.hpp>
#include <milg/graphics/swapchain.hpp>
#include <milg/graphics/vk_context.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace milg {
    class Application {
    public:
        Application(int argc, char **argv, const WindowCreateInfo &window_create_info);
        virtual ~Application();

        void run(float min_frametime = 0.0f);
        void close();

        void push_layer(class Layer *layer);

        static Application &get();

        const std::unique_ptr<Window>                  &window() const;
        const std::shared_ptr<graphics::VulkanContext> &context() const;
        const std::shared_ptr<graphics::Swapchain>     &swapchain() const;

        bool is_key_down(int32_t scan_code) const;
        bool is_mouse_button_down(int32_t button) const;

        VkCommandBuffer acquire_command_buffer();

        uint32_t frames_per_second() const;

    private:
        static Application *s_instance;

        std::array<bool, 512> m_keystates           = {};
        std::array<bool, 10>  m_mouse_button_states = {};

        std::unique_ptr<Window>                  m_window      = nullptr;
        std::shared_ptr<graphics::VulkanContext> m_context     = nullptr;
        std::shared_ptr<graphics::Swapchain>     m_swapchain   = nullptr;
        std::shared_ptr<ImGuiLayer>              m_imgui_layer = nullptr;
        std::vector<Layer *>                     m_layers      = {};
        bool                                     m_running     = true;

        uint32_t m_frames_per_second = 0;

        struct {
            const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

            std::vector<VkFence>     fences;
            std::vector<VkSemaphore> image_available_semaphores;
            std::vector<VkSemaphore> image_ready_semaphores;
            std::vector<VkSemaphore> layer_render_finished_semaphores;
            std::vector<VkSemaphore> render_finished_semaphores;

            VkCommandPool                             command_pool = VK_NULL_HANDLE;
            std::vector<std::vector<VkCommandBuffer>> leased_command_buffers;
            std::vector<VkCommandBuffer>              pre_frame_command_buffers;
            std::vector<VkCommandBuffer>              post_frame_command_buffers;

            uint32_t last_frame    = MAX_FRAMES_IN_FLIGHT - 1;
            uint32_t current_frame = 0;
        } m_frame_resources;
        void init_frame_resources();
        void destroy_frame_resources();

        void on_event(class Event &event);
    };

} // namespace milg
