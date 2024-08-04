#pragma once

#include <memory>
#include <vector>

#include "window.hpp"

namespace milg {
    class Application {
    public:
        Application(const WindowCreateInfo &window_create_info);
        virtual ~Application();

        void run();
        void close();

        void push_layer(class Layer *layer);

        static Application &get();

        const std::unique_ptr<Window>              &window() const;
        const std::shared_ptr<class VulkanContext> &context() const;
        const std::shared_ptr<class Swapchain>     &swapchain() const;

    private:
        static Application *s_instance;

        std::unique_ptr<Window>              m_window    = nullptr;
        std::shared_ptr<class VulkanContext> m_context   = nullptr;
        std::shared_ptr<class Swapchain>     m_swapchain = nullptr;
        std::vector<Layer *>                 m_layers    = {};
        bool                                 m_running   = true;

        void on_event(class Event &event);
    };

} // namespace milg
