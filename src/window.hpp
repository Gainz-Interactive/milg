#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace milg {
    struct WindowCreateInfo {
        std::string title     = "Untitled";
        int         width     = 800;
        int         height    = 600;
        bool        resizable = false;
    };

    struct Window {
        using EventCallbackFn = std::function<void(class Event &event)>;

    public:
        static std::unique_ptr<Window> create(const WindowCreateInfo &info);
        ~Window();

        void get_instance_extensions(std::vector<const char *> &extensions);
        void get_swapchain_surface(const std::shared_ptr<class VulkanContext> &ctx, void *surface);

        bool poll_events();

        void set_event_callback(const EventCallbackFn &callback);

        uint32_t width() const;
        uint32_t height() const;

        SDL_Window *handle() const;

    private:
        SDL_Window *m_handle = nullptr;
        int         m_width  = 0;
        int         m_height = 0;

        EventCallbackFn m_event_callback = nullptr;

        Window() = default;
    };
} // namespace milg
