#include "window.hpp"
#include "events.hpp"
#include "graphics/vk_context.hpp"

#include <SDL_events.h>
#include <SDL_video.h>
#include <cstdint>
#include <memory>
#include <milg.hpp>
#include <vector>

#include <SDL.h>
#include <SDL_vulkan.h>

namespace milg {
    std::unique_ptr<Window> Window::create(const WindowCreateInfo &info) {
        MILG_INFO("Creating window: {}x{}", info.width, info.height);
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            MILG_CRITICAL("Failed to initialize SDL: {}", SDL_GetError());
            return nullptr;
        }

        uint32_t flags = SDL_WINDOW_VULKAN;
        if (info.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        SDL_Window *sdl_window = SDL_CreateWindow(info.title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                                  info.width, info.height, flags);
        if (sdl_window == nullptr) {
            MILG_CRITICAL("Failed to create window: {}", SDL_GetError());
            return nullptr;
        }

        auto window      = std::unique_ptr<Window>(new Window());
        window->m_width  = info.width;
        window->m_height = info.height;
        window->m_handle = sdl_window;

        return window;
    }

    Window::~Window() {
        if (m_handle != nullptr) {
            SDL_DestroyWindow(m_handle);
        }
        SDL_Quit();
    }

    void Window::get_instance_extensions(std::vector<const char *> &extensions) {
        uint32_t count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(m_handle, &count, nullptr)) {
            return;
        }

        extensions.resize(extensions.size() + count);
        SDL_Vulkan_GetInstanceExtensions(m_handle, &count, &*(extensions.end() - count));
    }

    void Window::get_swapchain_surface(const std::shared_ptr<graphics::VulkanContext> &context, void *surface) {
        SDL_Vulkan_CreateSurface(m_handle, context->instance(), (VkSurfaceKHR *)surface);
    }

    bool Window::poll_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            RawEvent raw_event(&event);
            m_event_callback(raw_event);

            switch (event.type) {
            case SDL_QUIT: {
                WindowCloseEvent close_event;
                m_event_callback(close_event);

                return false;
            }
            case SDL_KEYDOWN: {
                KeyPressedEvent key_pressed_event(event.key.keysym.scancode, event.key.repeat);
                m_event_callback(key_pressed_event);
                break;
            }
            case SDL_KEYUP: {
                KeyReleasedEvent key_released_event(event.key.keysym.scancode);
                m_event_callback(key_released_event);
                break;
            }

            case SDL_TEXTINPUT: {
                KeyTypedEvent key_typed_event(event.text.text[0]);
                m_event_callback(key_typed_event);
                break;
            }

            case SDL_MOUSEMOTION: {
                MouseMovedEvent mouse_moved_event(event.motion.x, event.motion.y);
                m_event_callback(mouse_moved_event);
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                MousePressedEvent mouse_button_pressed_event(event.button.button);
                m_event_callback(mouse_button_pressed_event);
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                MouseReleasedEvent mouse_button_released_event(event.button.button);
                m_event_callback(mouse_button_released_event);
                break;
            }

            case SDL_MOUSEWHEEL: {
                MouseScrolledEvent mouse_scrolled_event(event.wheel.x, event.wheel.y);
                m_event_callback(mouse_scrolled_event);
                break;
            }

            case SDL_WINDOWEVENT: {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    WindowResizeEvent window_resize_event(event.window.data1, event.window.data2);
                    m_width  = event.window.data1;
                    m_height = event.window.data2;
                    m_event_callback(window_resize_event);
                }
                break;
            }
            }
        }

        return true;
    }

    void Window::set_event_callback(const EventCallbackFn &callback) {
        m_event_callback = callback;
    }

    uint32_t Window::width() const {
        return m_width;
    }

    uint32_t Window::height() const {
        return m_height;
    }

    SDL_Window *Window::handle() const {
        return m_handle;
    }
} // namespace milg
