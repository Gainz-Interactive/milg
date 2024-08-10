#include <SDL_mouse.h>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/random.hpp>
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
#include "sprite_batch.hpp"
#include "swapchain.hpp"
#include "texture.hpp"
#include "vk_context.hpp"
#include "window.hpp"

using namespace milg;

static std::filesystem::path               bindir;
static std::map<std::string, std::shared_ptr<audio::Sound>> sounds;

struct Particle {
    Sprite    sprite   = {};
    glm::vec2 velocity = {0.0f, 0.0f};
};

class Milg : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;

    std::shared_ptr<Texture> framebuffer = nullptr;

    std::shared_ptr<Texture>     texture      = nullptr;
    std::shared_ptr<Texture>     texture2     = nullptr;
    std::shared_ptr<SpriteBatch> sprite_batch = nullptr;

    bool                  repel          = false;
    float                 mouse_x        = 0.0f;
    float                 mouse_y        = 0.0f;
    float                 time           = 0.0f;
    const uint32_t        particle_count = 10'000;
    std::vector<Particle> particles;

    void on_attach() override {
        MILG_INFO("Starting Milg");
        context      = Application::get().context();
        auto &window = Application::get().window();

        this->framebuffer =
            Texture::create(context,
                            {
                                .format = VK_FORMAT_R8G8B8A8_UNORM,
                                .usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                            },
                            window->width(), window->height());

        TextureCreateInfo texture_info = {
            .format     = VK_FORMAT_R8G8B8A8_SRGB,
            .usage      = VK_IMAGE_USAGE_SAMPLED_BIT,
            .min_filter = VK_FILTER_NEAREST,
            .mag_filter = VK_FILTER_NEAREST,
        };
        this->texture      = Texture::load_from_file(context, texture_info, "data/one.png");
        this->texture2     = Texture::load_from_file(context, texture_info, "data/two.png");
        this->sprite_batch = SpriteBatch::create(bindir, context, framebuffer->format(), particle_count);

        for (uint32_t i = 0; i < particle_count; i++) {
            Sprite s;
            s.position = {glm::linearRand(0.0f, (float)framebuffer->width()),
                          glm::linearRand(0.0f, (float)framebuffer->height())};
            s.size     = {glm::linearRand(10.0f, 20.0f), glm::linearRand(10.0f, 20.0f)};
            s.color    = {glm::linearRand(0.3f, 1.0f), glm::linearRand(0.3f, 1.0f), glm::linearRand(0.3f, 1.0f), 0.4f};

            particles.push_back({
                .sprite   = s,
                .velocity = {glm::linearRand(-100.0f, 100.0f), glm::linearRand(-100.0f, 100.0f)},
            });
        }
    }

    void on_update(float delta) override {
        time += delta;

        auto command_buffer = Application::get().aquire_command_buffer();

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        context->device_table().vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        float     half_width  = framebuffer->width() * 0.5f;
        float     half_height = framebuffer->height() * 0.5f;
        glm::mat4 mat         = glm::ortho(-half_width, half_width, -half_height, half_height, -1.0f, 1.0f);
        mat                   = glm::translate(mat, {-half_width, -half_height, 0.0f});

        sprite_batch->reset();
        sprite_batch->begin_batch(mat);
        {
            auto i = 0;
            for (auto &p : particles) {
                p.sprite.rotation += 2.0f * delta;

                glm::vec2 a_pos           = {mouse_x, mouse_y};
                float     inv_dist        = 1.0f / glm::distance(p.sprite.position, a_pos);
                inv_dist                  = glm::clamp(inv_dist, 0.0f, 1.0f);
                glm::vec2 direction       = glm::normalize(a_pos - p.sprite.position);
                glm::vec2 target_velocity = direction * inv_dist * 100000.0f * (repel ? -1.0f : 1.0f);
                p.velocity += target_velocity * delta;

                p.sprite.position += p.velocity * delta;
                p.velocity *= 0.99f;

                sprite_batch->draw_sprite(p.sprite, i % 2 == 0 ? texture : texture2);
                i++;
            }
        }
        sprite_batch->build_batches(command_buffer);

        const VkExtent2D extent   = {framebuffer->width(), framebuffer->height()};
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
            .imageView          = framebuffer->image_view(),
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
        context->device_table().vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        context->device_table().vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        sprite_batch->render(command_buffer);
        context->device_table().vkCmdEndRendering(command_buffer);

        framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        Application::get().swapchain()->blit_to_current_image(command_buffer, framebuffer->handle(), extent);
        context->device_table().vkEndCommandBuffer(command_buffer);

        ImGui::Begin("Tryngle");
        if (ImGui::BeginTabBar("milg")) {
            if (ImGui::BeginTabItem("Graphics")) {
                ImGui::SeparatorText("Performance");
                ImGui::Text("Delta time: %.3f ms", delta);
                ImGui::Text("FPS: %d", Application::get().frames_per_second());

                ImGui::SeparatorText("Sprite Batch stats");
                ImGui::Text("Sprites: %d", sprite_batch->sprite_count());
                ImGui::Text("Batches: %d", sprite_batch->batch_count());
                ImGui::Text("Unique Textures: %d", sprite_batch->texture_count());

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio")) {
                ImGui::SeparatorText("Master volume");

                auto master_volume = audio::get_volume();

                if (ImGui::SliderFloat("##master_vol", &master_volume, 0.f, 1.f)) {
                    audio::set_volume(master_volume);
                }

                ImGui::SeparatorText("Loaded sounds");

                int        i              = 0;
                static int selected_index = 0;

                for (auto &[key, sound] : sounds) {
                    bool selected = selected_index == i;
                    auto volume = sound->get_volume();

                    if (ImGui::ArrowButton(std::format("##play_{}", key).c_str(), ImGuiDir_Right)) {
                        sound->play();
                    }
                    ImGui::SameLine();
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderFloat(std::format("##vol_{}", key).c_str(), &volume, 0.f, 1.f)) {
                        sound->set_volume(volume);
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
    }

    void on_event(Event &event) override {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<MouseMovedEvent>(BIND_EVENT_FN(Milg::on_mouse_moved));
        dispatcher.dispatch<MousePressedEvent>(BIND_EVENT_FN(Milg::on_mouse_button_pressed));
        dispatcher.dispatch<MouseReleasedEvent>(BIND_EVENT_FN(Milg::on_mouse_button_released));
    }

    bool on_mouse_moved(MouseMovedEvent &event) {
        mouse_x = event.x();
        mouse_y = event.y();

        return false;
    }

    bool on_mouse_button_pressed(MousePressedEvent &event) {
        if (event.button() == SDL_BUTTON_LEFT) {
            repel = true;
        }

        return false;
    }

    bool on_mouse_button_released(MouseReleasedEvent &event) {
        if (event.button() == SDL_BUTTON_LEFT) {
            repel = false;
        }

        return false;
    }

    void on_detach() override {
        MILG_INFO("Shutting down Milg");
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
        .width  = 1200,
        .height = 800,
    };

    Milglication app(window_info);

    sounds.insert({ "c1a0_sci_dis1d", std::make_shared<milg::audio::Sound>("data/c1a0_sci_dis1d.wav" )});
    sounds.insert({ "c1a0_sci_dis10a", std::make_shared<milg::audio::Sound>("data/c1a0_sci_dis10a.wav") });

    app.run();

    return 0;
}
