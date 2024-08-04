#include "application.hpp"
#include "audio.hpp"
#include "event.hpp"
#include "events.hpp"
#include "layer.hpp"
#include "swapchain.hpp"
#include "vk_context.hpp"

#include <chrono>
#include <memory>

namespace milg {
    Application *Application::s_instance = nullptr;

    Application::Application(const WindowCreateInfo &window_create_info) {
        Application::s_instance = this;

        m_window    = Window::create(window_create_info);
        m_context   = VulkanContext::create(m_window);
        m_swapchain = Swapchain::create(m_window, m_context);

        m_window->set_event_callback([this](Event &event) {
            on_event(event);
        });

        audio::init();
    }

    Application::~Application() {
        Application::s_instance = nullptr;

        audio::destroy();

        for (auto layer : m_layers) {
            layer->on_detach();
            delete layer;
        }
    }

    void Application::run() {
        auto current_time = std::chrono::high_resolution_clock::now();

        while (m_running) {
            auto  new_time = std::chrono::high_resolution_clock::now();
            float delta_time =
                std::chrono::duration<float, std::chrono::seconds::period>(new_time - current_time).count();
            current_time = new_time;

            if (!m_window->poll_events()) {
                close();
                break;
            }

            for (auto layer : m_layers) {
                layer->on_update(delta_time);
            }
        }
    }

    void Application::close() {
        m_running = false;
    }

    void Application::push_layer(Layer *layer) {
        m_layers.emplace_back(layer);
        layer->on_attach();
    }

    Application &Application::get() {
        return *Application::s_instance;
    }

    const std::unique_ptr<Window> &Application::window() const {
        return m_window;
    }

    const std::shared_ptr<VulkanContext> &Application::context() const {
        return m_context;
    }

    const std::shared_ptr<Swapchain> &Application::swapchain() const {
        return m_swapchain;
    }

    void Application::on_event(Event &event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &e) {
            close();
            return true;
        });

        for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
            if (event.handled) {
                break;
            }

            (*it)->on_event(event);
        }
    }
} // namespace milg
