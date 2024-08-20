#include <milg/core/application.hpp>

#include <milg/audio.hpp>
#include <milg/core/asset_store.hpp>
#include <milg/core/events.hpp>
#include <milg/core/layer.hpp>
#include <milg/core/logging.hpp>
#include <milg/graphics/swapchain.hpp>
#include <milg/graphics/vk_context.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <memory>

namespace milg {
    Application *Application::s_instance = nullptr;

    Application::Application(int argc, char **argv, const WindowCreateInfo &window_create_info) {
        Application::s_instance = this;

        m_window      = Window::create(window_create_info);
        m_context     = graphics::VulkanContext::create(m_window);
        m_swapchain   = graphics::Swapchain::create(m_window, m_context);
        m_imgui_layer = ImGuiLayer::create(m_swapchain, m_window, m_context);
        init_frame_resources();

        m_window->set_event_callback([this](Event &event) {
            on_event(event);
        });

        audio::init();

        auto bindir = std::filesystem::path(argv[0]).parent_path();

        asset_store::add_search_path((bindir / "data").lexically_normal());
        asset_store::add_search_path("data");
        asset_store::load_assets("data/assets.json");
    }

    Application::~Application() {
        Application::s_instance = nullptr;

        m_context->device_table().vkDeviceWaitIdle(m_context->device());

        destroy_frame_resources();
        for (auto layer : m_layers) {
            layer->on_detach();
            delete layer;
        }

        asset_store::unload_assets();
        audio::destroy();
    }

    void Application::run(float min_frametime) {
        auto     current_time   = std::chrono::high_resolution_clock::now();
        uint32_t elapsed_frames = 0;
        float    elapsed_time   = 0.0f;

        while (m_running) {
            auto  new_time = std::chrono::high_resolution_clock::now();
            float delta_time =
                std::chrono::duration<float, std::chrono::seconds::period>(new_time - current_time).count();
            current_time = new_time;

            if (min_frametime != 0.0f) {
                while (delta_time < min_frametime) {
                    new_time = std::chrono::high_resolution_clock::now();
                    delta_time +=
                        std::chrono::duration<float, std::chrono::seconds::period>(new_time - current_time).count();
                    current_time = new_time;
                }
            }

            if (!m_window->poll_events()) {
                close();
                break;
            }

            uint32_t frame_index      = m_frame_resources.current_frame;
            uint32_t last_frame_index = m_frame_resources.last_frame;

            std::vector<VkSubmitInfo> submit_infos;

            uint32_t swapchain_image_index = m_swapchain->acquire_next_image(
                m_frame_resources.image_available_semaphores[frame_index], VK_NULL_HANDLE);

            {
                auto &command_buffer = m_frame_resources.pre_frame_command_buffers[frame_index];
                const VkCommandBufferBeginInfo command_buffer_begin_info = {
                    .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext            = nullptr,
                    .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = nullptr,
                };
                VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));
                m_swapchain->transition_current_image(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                vkEndCommandBuffer(command_buffer);

                VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                const VkSubmitInfo   submit_info         = {
                              .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext                = nullptr,
                              .waitSemaphoreCount   = 1,
                              .pWaitSemaphores      = &m_frame_resources.image_available_semaphores[frame_index],
                              .pWaitDstStageMask    = &wait_dst_stage_mask,
                              .commandBufferCount   = 1,
                              .pCommandBuffers      = &command_buffer,
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores    = &m_frame_resources.image_ready_semaphores[frame_index],
                };
                submit_infos.push_back(submit_info);
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // TODO: This is not exactly a correct place to wait for the fence, resulting almost the same effect as a
            // waitIdle call, if performance tanks, this should be moved further down
            m_context->device_table().vkWaitForFences(m_context->device(), 1,
                                                      &m_frame_resources.fences[last_frame_index], VK_TRUE, UINT64_MAX);

            for (auto layer : m_layers) {
                layer->on_update(delta_time);
            }

            VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            const VkSubmitInfo   submit_info         = {
                          .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .pNext              = nullptr,
                          .waitSemaphoreCount = 1,
                          .pWaitSemaphores    = &m_frame_resources.image_ready_semaphores[frame_index],
                          .pWaitDstStageMask  = &wait_dst_stage_mask,
                          .commandBufferCount =
                    static_cast<uint32_t>(m_frame_resources.leased_command_buffers[frame_index].size()),
                          .pCommandBuffers      = m_frame_resources.leased_command_buffers[frame_index].data(),
                          .signalSemaphoreCount = 1,
                          .pSignalSemaphores    = &m_frame_resources.layer_render_finished_semaphores[frame_index],
            };
            submit_infos.push_back(submit_info);

            ImGui::Render();
            {
                auto &command_buffer = m_frame_resources.post_frame_command_buffers[frame_index];
                const VkCommandBufferBeginInfo command_buffer_begin_info = {
                    .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext            = nullptr,
                    .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = nullptr,
                };
                VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));
                m_swapchain->transition_current_image(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                const VkRenderingAttachmentInfo rendering_attachment_info = {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext              = nullptr,
                    .imageView          = m_swapchain->get_image(swapchain_image_index).view,
                    .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}},
                };
                const VkRenderingInfo rendering_info = {
                    .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                = nullptr,
                    .flags                = 0,
                    .renderArea           = {{0, 0}, m_swapchain->extent()},
                    .layerCount           = 1,
                    .viewMask             = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachments    = &rendering_attachment_info,
                    .pDepthAttachment     = nullptr,
                    .pStencilAttachment   = nullptr,
                };
                m_context->device_table().vkCmdBeginRendering(command_buffer, &rendering_info);
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
                m_context->device_table().vkCmdEndRendering(command_buffer);

                m_swapchain->transition_current_image(command_buffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                vkEndCommandBuffer(command_buffer);

                VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                const VkSubmitInfo   submit_info         = {
                              .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext                = nullptr,
                              .waitSemaphoreCount   = 1,
                              .pWaitSemaphores      = &m_frame_resources.layer_render_finished_semaphores[frame_index],
                              .pWaitDstStageMask    = &wait_dst_stage_mask,
                              .commandBufferCount   = 1,
                              .pCommandBuffers      = &command_buffer,
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores    = &m_frame_resources.render_finished_semaphores[frame_index],
                };
                submit_infos.push_back(submit_info);
            }

            if (m_frame_resources.leased_command_buffers[last_frame_index].size() > 0) {
                m_context->device_table().vkFreeCommandBuffers(
                    m_context->device(), m_frame_resources.command_pool,
                    static_cast<uint32_t>(m_frame_resources.leased_command_buffers[last_frame_index].size()),
                    m_frame_resources.leased_command_buffers[last_frame_index].data());

                m_frame_resources.leased_command_buffers[last_frame_index].clear();
            }

            m_context->device_table().vkResetFences(m_context->device(), 1, &m_frame_resources.fences[frame_index]);
            VK_CHECK(vkQueueSubmit(m_context->graphics_queue(), static_cast<uint32_t>(submit_infos.size()),
                                   submit_infos.data(), m_frame_resources.fences[frame_index]));
            m_swapchain->present_image(m_context->graphics_queue(),
                                       m_frame_resources.render_finished_semaphores[frame_index]);

            m_frame_resources.last_frame = m_frame_resources.current_frame;
            m_frame_resources.current_frame =
                (m_frame_resources.current_frame + 1) % m_frame_resources.MAX_FRAMES_IN_FLIGHT;

            elapsed_frames++;
            elapsed_time += delta_time;
            if (elapsed_time >= 1.0) {
                m_frames_per_second = elapsed_frames;

                elapsed_frames = 0;
                elapsed_time   = 0.0;
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

    const std::shared_ptr<graphics::VulkanContext> &Application::context() const {
        return m_context;
    }

    const std::shared_ptr<graphics::Swapchain> &Application::swapchain() const {
        return m_swapchain;
    }

    uint32_t Application::frames_per_second() const {
        return m_frames_per_second;
    }

    void Application::on_event(Event &event) {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowCloseEvent>([this](WindowCloseEvent &e) {
            close();
            return true;
        });

        dispatcher.dispatch<RawEvent>([this](RawEvent &e) {
            m_imgui_layer->process_event(e.raw_event());
            return false;
        });

        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent &e) {
            m_swapchain->resize(e.width(), e.height());
            return false;
        });

        dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent &e) {
            if (e.scan_code() >= static_cast<int32_t>(m_keystates.size())) {
                MILG_WARN("Key scan code out of range: {}", e.scan_code());
                return false;
            }

            m_keystates[e.scan_code()] = true;
            return false;
        });

        dispatcher.dispatch<KeyReleasedEvent>([this](KeyReleasedEvent &e) {
            if (e.scan_code() >= static_cast<int32_t>(m_keystates.size())) {
                MILG_WARN("Key scan code out of range: {}", e.scan_code());
                return false;
            }

            m_keystates[e.scan_code()] = false;
            return false;
        });

        dispatcher.dispatch<MousePressedEvent>([this](MousePressedEvent &e) {
            if (e.button() >= static_cast<int32_t>(m_mouse_button_states.size())) {
                MILG_WARN("Mouse button out of range: {}", e.button());
                return false;
            }

            m_mouse_button_states[e.button()] = true;
            return false;
        });

        dispatcher.dispatch<MouseReleasedEvent>([this](MouseReleasedEvent &e) {
            if (e.button() >= static_cast<int32_t>(m_mouse_button_states.size())) {
                MILG_WARN("Mouse button out of range: {}", e.button());
                return false;
            }

            m_mouse_button_states[e.button()] = false;
            return false;
        });

        for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
            if (event.handled) {
                break;
            }

            (*it)->on_event(event);
        }
    }

    bool Application::is_key_down(int32_t scan_code) const {
        if (scan_code >= static_cast<int32_t>(m_keystates.size())) {
            return false;
        }

        return m_keystates[scan_code];
    }

    bool Application::is_mouse_button_down(int32_t button) const {
        if (button >= static_cast<int32_t>(m_mouse_button_states.size())) {
            return false;
        }

        return m_mouse_button_states[button];
    }

    VkCommandBuffer Application::aquire_command_buffer() {
        VkCommandBuffer             command_buffer               = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo command_buffer_allocate_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = m_frame_resources.command_pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_CHECK(vkAllocateCommandBuffers(m_context->device(), &command_buffer_allocate_info, &command_buffer));

        m_frame_resources.leased_command_buffers[m_frame_resources.current_frame].push_back(command_buffer);

        return m_frame_resources.leased_command_buffers[m_frame_resources.current_frame].back();
    }

    void Application::init_frame_resources() {
        m_frame_resources.leased_command_buffers.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.pre_frame_command_buffers.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.post_frame_command_buffers.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.fences.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.image_available_semaphores.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.image_ready_semaphores.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.layer_render_finished_semaphores.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);
        m_frame_resources.render_finished_semaphores.resize(m_frame_resources.MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < m_frame_resources.MAX_FRAMES_IN_FLIGHT; ++i) {
            const VkSemaphoreCreateInfo semaphore_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
            };

            for (auto &semaphore :
                 {&m_frame_resources.image_available_semaphores[i],
                  &m_frame_resources.layer_render_finished_semaphores[i],
                  &m_frame_resources.render_finished_semaphores[i], &m_frame_resources.image_ready_semaphores[i]}) {
                VK_CHECK(vkCreateSemaphore(m_context->device(), &semaphore_info, nullptr, semaphore));
            }
        }

        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (auto &fence : m_frame_resources.fences) {
            VK_CHECK(vkCreateFence(m_context->device(), &fence_info, nullptr, &fence));
        }

        VkCommandPoolCreateInfo command_pool_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_context->graphics_queue_family_index(),
        };
        VK_CHECK(
            vkCreateCommandPool(m_context->device(), &command_pool_info, nullptr, &m_frame_resources.command_pool));

        VkCommandBufferAllocateInfo command_buffer_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = m_frame_resources.command_pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(m_frame_resources.pre_frame_command_buffers.size()),
        };

        VK_CHECK(vkAllocateCommandBuffers(m_context->device(), &command_buffer_info,
                                          m_frame_resources.pre_frame_command_buffers.data()));

        command_buffer_info.commandBufferCount =
            static_cast<uint32_t>(m_frame_resources.post_frame_command_buffers.size());
        VK_CHECK(vkAllocateCommandBuffers(m_context->device(), &command_buffer_info,
                                          m_frame_resources.post_frame_command_buffers.data()));
    }

    void Application::destroy_frame_resources() {
        for (size_t i = 0; i < m_frame_resources.MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(m_context->device(), m_frame_resources.image_available_semaphores[i], nullptr);
            vkDestroySemaphore(m_context->device(), m_frame_resources.layer_render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(m_context->device(), m_frame_resources.render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(m_context->device(), m_frame_resources.image_ready_semaphores[i], nullptr);
            vkDestroyFence(m_context->device(), m_frame_resources.fences[i], nullptr);
        }

        vkDestroyCommandPool(m_context->device(), m_frame_resources.command_pool, nullptr);
    }
} // namespace milg
