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
#include "imgui.h"
#include "layer.hpp"
#include "logging.hpp"
#include "pipeline.hpp"
#include "sprite_batch.hpp"
#include "swapchain.hpp"
#include "texture.hpp"
#include "vk_context.hpp"
#include "window.hpp"

using namespace milg;

static std::filesystem::path               bindir;
static std::map<std::string, audio::Sound> sounds;

struct Particle {
    Sprite    sprite   = {};
    glm::vec2 velocity = {0.0f, 0.0f};
};

VkShaderModule load_shader_module(const std::filesystem::path &path, const std::shared_ptr<VulkanContext> &context) {
    MILG_INFO("Loading shader module from file: {}", path.string());
    std::ifstream file(path);
    if (!file.is_open()) {
        file = std::ifstream(bindir / path);
    }
    if (!file.is_open()) {
        MILG_ERROR("Failed to open file: {}", path.string());
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

std::tuple<VkPipeline, VkPipelineLayout> create_compute_pipeline(const std::shared_ptr<VulkanContext> &context,
                                                                 const std::filesystem::path          &shader_path,
                                                                 VkDescriptorSetLayout                 set_layout,
                                                                 uint32_t push_constant_size = 0) {
    auto shader_module = load_shader_module(shader_path, context);

    VkPipelineShaderStageCreateInfo shader_stage_info = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName  = "main",
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = push_constant_size,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 1,
        .pSetLayouts            = &set_layout,
        .pushConstantRangeCount = push_constant_size > 0 ? 1u : 0u,
        .pPushConstantRanges    = push_constant_size > 0 ? &push_constant_range : nullptr,
    };

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VK_CHECK(context->device_table().vkCreatePipelineLayout(context->device(), &pipeline_layout_info, nullptr,
                                                            &pipeline_layout));

    VkComputePipelineCreateInfo pipeline_info = {
        .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext              = nullptr,
        .flags              = 0,
        .stage              = shader_stage_info,
        .layout             = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = 0,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(context->device_table().vkCreateComputePipelines(context->device(), VK_NULL_HANDLE, 1, &pipeline_info,
                                                              nullptr, &pipeline));

    return {pipeline, pipeline_layout};
}

class Milg : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;

    struct {
        glm::vec2 inverse_resolution = {0.0f, 0.0f};
        float     time               = 0.0f;
        float     bounce_factor      = 0.5f;
        float     blend_factor       = 1.0 - 0.6f;
        float     _pad               = 0.0f;
    } raytrace_pass_constants;

    struct {
        float enable_denoise    = 1.0f;
        float exposure          = 5.0f;
        float denoise_sigma     = 5.0f;
        float denoise_k_sigma   = 1.0f;
        float denoise_threshold = 0.05f;
    } composite_pass_constants;

    std::shared_ptr<Texture> framebuffer = nullptr;

    std::shared_ptr<Texture>     texture       = nullptr;
    std::shared_ptr<Texture>     texture2      = nullptr;
    std::shared_ptr<Texture>     bg_texture    = nullptr;
    std::shared_ptr<Texture>     map_texture   = nullptr;
    std::shared_ptr<Texture>     noise_texture = nullptr;
    std::shared_ptr<SpriteBatch> sprite_batch  = nullptr;

    std::shared_ptr<PipelineFactory> pipeline_factory        = nullptr;
    Pipeline                        *voronoi_seed_pipeline   = nullptr;
    Pipeline                        *voronoi_pipeline        = nullptr;
    Pipeline                        *distance_field_pipeline = nullptr;
    Pipeline                        *raytrace_pipeline       = nullptr;
    Pipeline                        *composite_pipeline      = nullptr;

    bool                  repel          = false;
    float                 mouse_x        = 0.0f;
    float                 mouse_y        = 0.0f;
    float                 time           = 0.0f;
    const uint32_t        particle_count = 10;
    std::vector<Particle> particles;

    void on_attach() override {
        MILG_INFO("Starting Milg");
        context      = Application::get().context();
        auto &window = Application::get().window();

        this->framebuffer =
            Texture::create(context,
                            {
                                .format = VK_FORMAT_R8G8B8A8_UNORM,
                                .usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            },
                            window->width(), window->height());

        TextureCreateInfo texture_info = {
            .format     = VK_FORMAT_R8G8B8A8_UNORM,
            .usage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .min_filter = VK_FILTER_NEAREST,
            .mag_filter = VK_FILTER_NEAREST,
        };
        this->texture       = Texture::load_from_file(context, texture_info, "data/one.png");
        this->texture2      = Texture::load_from_file(context, texture_info, "data/two.png");
        this->map_texture   = Texture::load_from_file(context, texture_info, "data/map.png");
        this->bg_texture    = Texture::load_from_file(context, texture_info, "data/bg.png");
        this->noise_texture = Texture::load_from_file(context, texture_info, "data/noise.png");

        this->sprite_batch = SpriteBatch::create(bindir, context, framebuffer->format(), 10000);

        this->pipeline_factory   = PipelineFactory::create(bindir, context);
        this->composite_pipeline = this->pipeline_factory->create_compute_pipeline(
            "composite", "data/shaders/composite.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R8G8B8A8_UNORM, .width = window->width(), .height = window->height()}},
            4, sizeof(composite_pass_constants));
        this->raytrace_pipeline = this->pipeline_factory->create_compute_pipeline(
            "raytrace", "data/shaders/raytrace.comp.spv",
            {PipelineOutputDescription{
                 .format = VK_FORMAT_R16G16B16A16_SFLOAT, .width = window->width(), .height = window->height()},
             PipelineOutputDescription{
                 .format = VK_FORMAT_R16G16B16A16_SFLOAT, .width = window->width(), .height = window->height()}},
            5, sizeof(raytrace_pass_constants));
        this->distance_field_pipeline = this->pipeline_factory->create_compute_pipeline(
            "distance_field", "data/shaders/distance_field.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R16_SFLOAT, .width = window->width(), .height = window->height()}},
            2);
        this->voronoi_pipeline = this->pipeline_factory->create_compute_pipeline(
            "voronoi", "data/shaders/voronoi.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT, .width = window->width(), .height = window->height()}},
            2, sizeof(float) * 6);
        this->voronoi_seed_pipeline = this->pipeline_factory->create_compute_pipeline(
            "voronoi_seed", "data/shaders/voronoi_seed.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R16G16B16A16_SFLOAT, .width = window->width(), .height = window->height()}},
            2);

        for (uint32_t i = 0; i < particle_count; i++) {
            Sprite s;
            s.position = {glm::linearRand(0.0f, (float)framebuffer->width()),
                          glm::linearRand(0.0f, (float)framebuffer->height())};
            float size = glm::linearRand(5.0f, 10.0f);
            s.size     = {size, size},
            s.color    = {glm::linearRand(0.3f, 1.0f), glm::linearRand(0.3f, 1.0f), glm::linearRand(0.3f, 1.0f), 1.0f};

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
        Sprite s;
        s.position = {map_texture->width() * 0.5f, 0.0};
        s.color    = {1.0f, 1.0f, 1.0f, 1.0f};
        s.size     = {map_texture->width(), map_texture->height()};
        sprite_batch->draw_sprite(s, map_texture);
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
            .clearValue         = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
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

        {
            auto pipeline = voronoi_seed_pipeline;
            auto output   = pipeline->output_buffers[0];

            framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, framebuffer);
            pipeline->bind_texture(context, command_buffer, 1, output);

            pipeline->bind_pipeline(context, command_buffer);
            context->device_table().vkCmdDispatch(command_buffer, output->width() / 16, output->height() / 16, 1);
        }

        std::shared_ptr<Texture> voronoi_output_buffer = nullptr;
        {
            auto pipeline = voronoi_pipeline;
            auto output   = voronoi_seed_pipeline->output_buffers[0];
            auto output2  = pipeline->output_buffers[0];

            auto pass_count = glm::ceil(glm::log2(glm::max((float)output->width(), (float)output->height())));

            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            output2->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, output);
            pipeline->bind_texture(context, command_buffer, 1, output2);

            pipeline->bind_pipeline(context, command_buffer);
            for (int i = 0; i < pass_count; i++) {
                float offset          = glm::pow(2, pass_count - i - 1);
                voronoi_output_buffer = i % 2 == 0 ? output : output2;
                struct {
                    glm::vec2 inverse_resolution;
                    glm::vec2 offset;
                    glm::vec2 misc;
                } push_constants = {
                    .inverse_resolution = {1.0f / output->width(), 1.0f / output->height()},
                    .offset             = {offset, offset},
                    .misc               = {i % 2 == 0, 0.0f},
                };

                pipeline->set_push_constants(context, command_buffer, sizeof(push_constants), &push_constants);
                context->device_table().vkCmdDispatch(command_buffer, output->width() / 16, output->height() / 16, 1);

                output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
                output2->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            }
        }

        {
            auto pipeline        = distance_field_pipeline;
            auto output          = pipeline->output_buffers[0];
            auto distance_buffer = voronoi_pipeline->output_buffers[0];

            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            distance_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, distance_buffer);
            pipeline->bind_texture(context, command_buffer, 1, output);

            pipeline->bind_pipeline(context, command_buffer);
            context->device_table().vkCmdDispatch(command_buffer, output->width() / 16, output->height() / 16, 1);
        }
        {
            auto pipeline       = raytrace_pipeline;
            auto output         = pipeline->output_buffers[0];
            auto history_output = pipeline->output_buffers[1];
            auto df_pass_buffer = distance_field_pipeline->output_buffers[0];

            history_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            history_output->blit_from(output, command_buffer);

            df_pass_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            noise_texture->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            history_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, df_pass_buffer);
            pipeline->bind_texture(context, command_buffer, 1, framebuffer);
            pipeline->bind_texture(context, command_buffer, 2, noise_texture);
            pipeline->bind_texture(context, command_buffer, 3, history_output);
            pipeline->bind_texture(context, command_buffer, 4, output);

            raytrace_pass_constants.inverse_resolution = {1.0f / output->width(), 1.0f / output->height()};
            raytrace_pass_constants.time               = time;

            pipeline->bind_pipeline(context, command_buffer, sizeof(raytrace_pass_constants), &raytrace_pass_constants);
            context->device_table().vkCmdDispatch(command_buffer, output->width() / 16, output->height() / 16, 1);
        }

        {
            auto pipeline  = composite_pipeline;
            auto rt_output = raytrace_pipeline->output_buffers[0];
            auto output    = pipeline->output_buffers[0];

            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            rt_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            bg_texture->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, bg_texture);
            pipeline->bind_texture(context, command_buffer, 1, rt_output);
            pipeline->bind_texture(context, command_buffer, 2, framebuffer);
            pipeline->bind_texture(context, command_buffer, 3, output);

            pipeline->bind_pipeline(context, command_buffer, sizeof(composite_pass_constants),
                                    &composite_pass_constants);
            context->device_table().vkCmdDispatch(command_buffer, output->width() / 16, output->height() / 16, 1);

            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            Application::get().swapchain()->blit_to_current_image(
                command_buffer, output->handle(), {.width = output->width(), .height = output->height()});
        }

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

                ImGui::SeparatorText("RT Options");
                ImGui::SliderFloat("Bounce Factor", &raytrace_pass_constants.bounce_factor, 0.0f, 1.0f);
                ImGui::SliderFloat("Blend Factor", &raytrace_pass_constants.blend_factor, 0.01f, 0.99f);

                ImGui::SeparatorText("Composite Options");
                bool enable_denoise = composite_pass_constants.enable_denoise == 1.0f;
                if (ImGui::Checkbox("Denoise Light", &enable_denoise)) {
                    composite_pass_constants.enable_denoise = enable_denoise ? 1.0f : 0.0f;
                }
                ImGui::SliderFloat("Exposure", &composite_pass_constants.exposure, 0.0f, 10.0f);
                ImGui::SliderFloat("Denoise Sigma", &composite_pass_constants.denoise_sigma, 0.0f, 10.0f);
                ImGui::SliderFloat("Denoise K Sigma", &composite_pass_constants.denoise_k_sigma, 0.0f, 10.0f);
                ImGui::SliderFloat("Denoise Threshold", &composite_pass_constants.denoise_threshold, 0.0f, 1.0f);

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Audio")) {
                auto master_volume = audio::get_volume();

                if (ImGui::SliderFloat("Master volume", &master_volume, 0.f, 1.f)) {
                    audio::set_volume(master_volume);
                }

                ImGui::SeparatorText("Loaded sounds");

                int        i              = 0;
                static int selected_index = 0;

                for (auto &[key, sound] : sounds) {
                    bool selected = selected_index == i;
                    auto volume   = sound.get_volume();

                    if (ImGui::Button(std::format("Play##{}", key).c_str())) {
                        sound.play();
                    }
                    ImGui::SameLine();
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderFloat(std::format("##vol_{}", key).c_str(), &volume, 0.f, 1.f)) {
                        sound.set_volume(volume);
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
        .width  = 1600,
        .height = 1008,
    };

    Milglication app(window_info);

    sounds.emplace("c1a0_sci_dis1d", "data/c1a0_sci_dis1d.wav");
    sounds.emplace("c1a0_sci_dis10a", "data/c1a0_sci_dis10a.wav");

    app.run();

    return 0;
}
