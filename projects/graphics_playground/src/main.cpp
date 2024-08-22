#include <milg/graphics.hpp>
#include <milg/graphics/map.hpp>
#include <milg/graphics/pipeline.hpp>
#include <milg/graphics/sprite_batch.hpp>
#include <milg/graphics/texture.hpp>
#include <milg/milg.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

#include <cstdint>

using namespace milg;
using namespace milg::graphics;

constexpr uint32_t WORKGROUP_SIZE = 32;
uint32_t           dispatch_size(uint32_t work_size) {
    return (work_size + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
}

class RTLight : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;

    struct {
        glm::vec2 inverse_resolution = {0.0f, 0.0f};
        glm::vec2 resolution         = {0.0f, 0.0f};
        float     time               = 0.0f;
        float     bounce_factor      = 1.0f;
        float     blend_factor       = 1.0 - 0.6f;
        float     scale_modifier     = 0.0f;
    } raytrace_pass_constants;

    struct {
        float exposure = 5.0f;
    } composite_pass_constants;

    struct {
        float sample_num            = 80.0f;
        float distribution_bias     = 0.6f;
        float pixel_multiplier      = 1.5f;
        float inverse_hue_tolerance = 5.0f;
    } rt_upscale_pass_constants;

    std::shared_ptr<Texture> albedo_buffer   = nullptr;
    std::shared_ptr<Texture> emissive_buffer = nullptr;

    std::shared_ptr<Texture>     albedo_texture   = nullptr;
    std::shared_ptr<Texture>     emissive_texture = nullptr;
    std::shared_ptr<Texture>     noise_texture    = nullptr;
    std::shared_ptr<Texture>     light_texture    = nullptr;
    std::shared_ptr<SpriteBatch> sprite_batch     = nullptr;

    uint64_t                         frame_index             = 0;
    float                            rt_scale                = 0.5f;
    std::shared_ptr<PipelineFactory> pipeline_factory        = nullptr;
    Pipeline                        *voronoi_seed_pipeline   = nullptr;
    Pipeline                        *voronoi_pipeline        = nullptr;
    Pipeline                        *distance_field_pipeline = nullptr;
    Pipeline                        *noise_seed_pipeline     = nullptr;
    Pipeline                        *raytrace_pipeline       = nullptr;
    Pipeline                        *rt_upscale_pipeline     = nullptr;
    Pipeline                        *composite_pipeline      = nullptr;

    glm::vec2 mouse_position = {0.0f, 0.0f};
    float     time           = 0.0f;

    void on_attach() override {
        MILG_INFO("Initializing Grapchiks");

        context      = Application::get().context();
        auto &window = Application::get().window();

        this->emissive_buffer =
            Texture::create(context,
                            {
                                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                .usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            },
                            window->width(), window->height());

        this->albedo_buffer =
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
        {
            auto data            = asset_store::get_asset("map");
            this->albedo_texture = Texture::load_from_data(context, texture_info, data->get_data(), data->get_size());
        }
        {
            auto data              = asset_store::get_asset("map_emissive");
            this->emissive_texture = Texture::load_from_data(context, texture_info, data->get_data(), data->get_size());
        }
        {
            auto data           = asset_store::get_asset("noise");
            this->noise_texture = Texture::load_from_data(context, texture_info, data->get_data(), data->get_size());
        }
        {
            auto data           = asset_store::get_asset("light");
            this->light_texture = Texture::load_from_data(context, texture_info, data->get_data(), data->get_size());
        }

        this->sprite_batch = SpriteBatch::create(context, albedo_buffer->format(), 10000);

        this->pipeline_factory      = PipelineFactory::create(context);
        this->voronoi_seed_pipeline = this->pipeline_factory->create_compute_pipeline(
            "voronoi_seed", "voronoi_seed.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R8G8B8A8_UNORM, .width = window->width(), .height = window->height()}},
            2);
        this->voronoi_pipeline = this->pipeline_factory->create_compute_pipeline(
            "voronoi", "voronoi.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R8G8B8A8_UNORM, .width = window->width(), .height = window->height()}},
            2, sizeof(float) * 6);
        this->distance_field_pipeline = this->pipeline_factory->create_compute_pipeline(
            "distance_field", "distance_field.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R8G8_UNORM, .width = window->width(), .height = window->height()}},
            2);
        this->noise_seed_pipeline = this->pipeline_factory->create_compute_pipeline(
            "noise_seed", "noise_seed.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R8_UNORM, .width = noise_texture->width(), .height = noise_texture->height()}},
            2, sizeof(float));
        this->raytrace_pipeline = this->pipeline_factory->create_compute_pipeline(
            "raytrace", "raytrace.comp.spv",
            {PipelineOutputDescription{.format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                       .width  = static_cast<uint32_t>(window->width() * rt_scale),
                                       .height = static_cast<uint32_t>(window->height() * rt_scale)},
             PipelineOutputDescription{.format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                       .width  = static_cast<uint32_t>(window->width() * rt_scale),
                                       .height = static_cast<uint32_t>(window->height() * rt_scale)}},
            6, sizeof(raytrace_pass_constants));
        this->rt_upscale_pipeline = this->pipeline_factory->create_compute_pipeline(
            "rt_upscale", "rt_upscale.comp.spv",
            {PipelineOutputDescription{.format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                       .width  = static_cast<uint32_t>(window->width() * rt_scale),
                                       .height = static_cast<uint32_t>(window->height() * rt_scale)},
             PipelineOutputDescription{
                 .format = VK_FORMAT_R16G16B16A16_SFLOAT, .width = window->width(), .height = window->height()}},
            2, sizeof(rt_upscale_pass_constants));
        this->composite_pipeline = this->pipeline_factory->create_compute_pipeline(
            "composite", "composite.comp.spv",
            {PipelineOutputDescription{
                .format = VK_FORMAT_R8G8B8A8_UNORM, .width = window->width(), .height = window->height()}},
            4, sizeof(composite_pass_constants));
    }

    void on_update(float delta) override {
        time += delta;

        auto command_buffer = Application::get().acquire_command_buffer();

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        context->device_table().vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        pipeline_factory->begin_frame(command_buffer);

        albedo_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        emissive_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        float     half_width  = albedo_buffer->width() * 0.5f;
        float     half_height = albedo_buffer->height() * 0.5f;
        glm::mat4 mat         = glm::ortho(-half_width, half_width, -half_height, half_height, -1.0f, 1.0f);
        mat                   = glm::translate(mat, {-half_width, -half_height, 0.0f});

        sprite_batch->reset();
        sprite_batch->begin_batch(mat);
        Sprite s;
        s.position = {albedo_texture->width() * 0.5, 0.0};
        s.color    = {1.0f, 1.0f, 1.0f, 1.0f};
        s.size     = {albedo_texture->width(), albedo_texture->height()};

        Sprite occluder;
        occluder.position = {200, 200};
        occluder.color    = {3.0f, 3.0f, 3.0f, 1.0f};
        occluder.size     = {10, 100};
        occluder.rotation = time * 5;
        sprite_batch->draw_sprite(occluder, light_texture);

        occluder.rotation = (time + 180) * 5;
        sprite_batch->draw_sprite(occluder, light_texture);

        sprite_batch->draw_sprite(s, albedo_texture);
        sprite_batch->build_batches(command_buffer);

        const VkExtent2D extent   = {albedo_buffer->width(), albedo_buffer->height()};
        const VkRect2D   scissor  = {{0, 0}, extent};
        const VkViewport viewport = {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = (float)extent.width,
            .height   = (float)extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const std::array<VkRenderingAttachmentInfo, 2> rendering_attachment_infos = {
            VkRenderingAttachmentInfo{
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = albedo_buffer->image_view(),
                .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode        = VK_RESOLVE_MODE_NONE,
                .resolveImageView   = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue         = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
            },
            VkRenderingAttachmentInfo{
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = emissive_buffer->image_view(),
                .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode        = VK_RESOLVE_MODE_NONE,
                .resolveImageView   = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue         = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}},
            }};

        const VkRenderingInfo rendering_info = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {{0, 0}, extent},
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = static_cast<uint32_t>(rendering_attachment_infos.size()),
            .pColorAttachments    = rendering_attachment_infos.data(),
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

            pipeline->begin(context, command_buffer);
            emissive_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, emissive_buffer);
            pipeline->bind_texture(context, command_buffer, 1, output);

            context->device_table().vkCmdDispatch(command_buffer, dispatch_size(output->width()),
                                                  dispatch_size(output->height()), 1);
            pipeline->end(context, command_buffer);
        }

        std::shared_ptr<Texture> voronoi_output_buffer = nullptr;
        {
            auto pipeline = voronoi_pipeline;
            auto output   = voronoi_seed_pipeline->output_buffers[0];
            auto output2  = pipeline->output_buffers[0];

            auto pass_count = glm::ceil(glm::log2(glm::max((float)output->width(), (float)output->height())));

            pipeline->begin(context, command_buffer);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            output2->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, output);
            pipeline->bind_texture(context, command_buffer, 1, output2);

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
                context->device_table().vkCmdDispatch(command_buffer, dispatch_size(output->width()),
                                                      dispatch_size(output->height()), 1);

                output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
                output2->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            }
            pipeline->end(context, command_buffer);
        }

        {
            auto pipeline        = distance_field_pipeline;
            auto output          = pipeline->output_buffers[0];
            auto distance_buffer = voronoi_pipeline->output_buffers[0];

            pipeline->begin(context, command_buffer);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            distance_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, distance_buffer);
            pipeline->bind_texture(context, command_buffer, 1, output);

            context->device_table().vkCmdDispatch(command_buffer, dispatch_size(output->width()),
                                                  dispatch_size(output->height()), 1);
            pipeline->end(context, command_buffer);
        }

        {
            auto pipeline = noise_seed_pipeline;
            auto output   = pipeline->output_buffers[0];

            pipeline->begin(context, command_buffer);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            noise_texture->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, noise_texture);
            pipeline->bind_texture(context, command_buffer, 1, output);

            struct {
                float time;
            } push_constants = {
                .time = time,
            };

            pipeline->set_push_constants(context, command_buffer, sizeof(push_constants), &push_constants);
            context->device_table().vkCmdDispatch(command_buffer, dispatch_size(output->width()),
                                                  dispatch_size(output->height()), 1);
            pipeline->end(context, command_buffer);
        }

        {
            auto pipeline       = raytrace_pipeline;
            auto output         = pipeline->output_buffers[0];
            auto history_output = pipeline->output_buffers[1];
            auto df_pass_buffer = distance_field_pipeline->output_buffers[0];
            auto noise          = noise_seed_pipeline->output_buffers[0];

            pipeline->begin(context, command_buffer, sizeof(raytrace_pass_constants), &raytrace_pass_constants);
            df_pass_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            emissive_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            noise->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            history_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            albedo_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            if (frame_index == 0) {
                const VkClearColorValue       clear_color       = {{0.0f, 0.0f, 0.0f, 1.0f}};
                const VkImageSubresourceRange subresource_range = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                };
                context->device_table().vkCmdClearColorImage(command_buffer, history_output->handle(),
                                                             VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1,
                                                             &subresource_range);
            } else {
                if (frame_index % 2 == 0) {
                    history_output = raytrace_pipeline->output_buffers[0];
                    output         = raytrace_pipeline->output_buffers[1];
                } else {
                    history_output = raytrace_pipeline->output_buffers[1];
                    output         = raytrace_pipeline->output_buffers[0];
                }
            };

            pipeline->bind_texture(context, command_buffer, 0, df_pass_buffer);
            pipeline->bind_texture(context, command_buffer, 1, emissive_buffer);
            pipeline->bind_texture(context, command_buffer, 2, albedo_buffer);
            pipeline->bind_texture(context, command_buffer, 3, noise);
            pipeline->bind_texture(context, command_buffer, 4, history_output);
            pipeline->bind_texture(context, command_buffer, 5, output);

            raytrace_pass_constants.inverse_resolution = {1.0f / output->width(), 1.0f / output->height()};
            raytrace_pass_constants.resolution         = {output->width(), output->height()};
            raytrace_pass_constants.time               = time;
            raytrace_pass_constants.scale_modifier     = rt_scale;

            context->device_table().vkCmdDispatch(command_buffer, dispatch_size(output->width()),
                                                  dispatch_size(output->height()), 1);
            pipeline->end(context, command_buffer);
        }

        {
            auto pipeline        = rt_upscale_pipeline;
            auto rt_output       = raytrace_pipeline->output_buffers[0];
            auto denoised_output = pipeline->output_buffers[0];
            auto upscaled_output = pipeline->output_buffers[1];

            pipeline->begin(context, command_buffer);
            denoised_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            rt_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, rt_output);
            pipeline->bind_texture(context, command_buffer, 1, denoised_output);

            pipeline->set_push_constants(context, command_buffer, sizeof(rt_upscale_pass_constants),
                                         &rt_upscale_pass_constants);
            context->device_table().vkCmdDispatch(command_buffer, dispatch_size(denoised_output->width()),
                                                  dispatch_size(denoised_output->height()), 1);
            pipeline->end(context, command_buffer);

            denoised_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            upscaled_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            upscaled_output->blit_from(denoised_output, command_buffer);
        }

        {
            auto pipeline  = composite_pipeline;
            auto rt_output = rt_upscale_pipeline->output_buffers[1];
            auto output    = pipeline->output_buffers[0];

            pipeline->begin(context, command_buffer, sizeof(composite_pass_constants), &composite_pass_constants);
            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            albedo_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            rt_output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);
            emissive_buffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_GENERAL);

            pipeline->bind_texture(context, command_buffer, 0, albedo_buffer);
            pipeline->bind_texture(context, command_buffer, 1, emissive_buffer);
            pipeline->bind_texture(context, command_buffer, 2, rt_output);
            pipeline->bind_texture(context, command_buffer, 3, output);

            context->device_table().vkCmdDispatch(command_buffer, dispatch_size(output->width()),
                                                  dispatch_size(output->height()), 1);
            pipeline->end(context, command_buffer);

            output->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            Application::get().swapchain()->blit_to_current_image(
                command_buffer, output->handle(), {.width = output->width(), .height = output->height()});
        }

        pipeline_factory->end_frame(command_buffer);
        context->device_table().vkEndCommandBuffer(command_buffer);

        ImGui::Begin("Graphics");
        if (ImGui::BeginTabBar("##graphics_tab_bar")) {
            if (ImGui::BeginTabItem("Graphics")) {
                ImGui::SeparatorText("Performance");
                ImGui::Text("Delta time: %.3f ms", delta);
                ImGui::Text("FPS: %d", Application::get().frames_per_second());

                ImGui::SeparatorText("Sprite Batch stats");
                ImGui::Text("Sprites: %d", sprite_batch->sprite_count());
                ImGui::Text("Batches: %d", sprite_batch->batch_count());
                ImGui::Text("Unique Textures: %d", sprite_batch->texture_count());
                if (ImGui::CollapsingHeader("Render Timings")) {
                    float total_time = pipeline_factory->pre_execution_time();
                    ImGui::Text("scene: %.3f ms", pipeline_factory->pre_execution_time());
                    for (auto &[name, pipeline] : pipeline_factory->get_pipelines()) {
                        ImGui::Text("%s: %.3f ms", name.c_str(), pipeline.execution_time);
                        total_time += pipeline.execution_time;
                    }
                    ImGui::Separator();
                    ImGui::Text("Total: %.3f ms", total_time);
                }

                ImGui::SeparatorText("RT Options");
                ImGui::SliderFloat("Bounce Factor", &raytrace_pass_constants.bounce_factor, 0.0f, 1.0f);
                ImGui::SliderFloat("Blend Factor", &raytrace_pass_constants.blend_factor, 0.01f, 0.99f);

                ImGui::SeparatorText("Composite Options");
                ImGui::SliderFloat("Exposure", &composite_pass_constants.exposure, 0.0f, 10.0f);

                ImGui::SeparatorText("Denoise Options");
                ImGui::SliderFloat("Sample Count", &rt_upscale_pass_constants.sample_num, 1.0f, 120.0f);
                ImGui::SliderFloat("Distribution Bias", &rt_upscale_pass_constants.distribution_bias, 0.0f, 1.0f);
                ImGui::SliderFloat("Pixel Multiplier", &rt_upscale_pass_constants.pixel_multiplier, 1.0f, 3.0f);
                ImGui::SliderFloat("Iverse Hue Tolerance", &rt_upscale_pass_constants.inverse_hue_tolerance, 2.0f,
                                   30.0f);

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        frame_index++;
    }

    void on_event(Event &event) override {
        EventDispatcher dispatcher(event);

        dispatcher.dispatch<MouseMovedEvent>(BIND_EVENT_FN(RTLight::on_mouse_moved));
        dispatcher.dispatch<MousePressedEvent>(BIND_EVENT_FN(RTLight::on_mouse_button_pressed));
        dispatcher.dispatch<MouseReleasedEvent>(BIND_EVENT_FN(RTLight::on_mouse_button_released));
    }

    bool on_mouse_moved(MouseMovedEvent &event) {
        mouse_position = {event.x(), event.y()};

        return false;
    }

    bool on_mouse_button_pressed(MousePressedEvent &event) {
        return false;
    }

    bool on_mouse_button_released(MouseReleasedEvent &event) {
        return false;
    }

    void on_detach() override {
        MILG_INFO("Goodbye Grapchiks");
    }
};

class GraphicsPlayground : public Application {
public:
    GraphicsPlayground(int argc, char **argv, const WindowCreateInfo &window_info)
        : Application(argc, argv, window_info) {
        auto bindir = std::filesystem::path(argv[0]).parent_path();

        asset_store::add_search_path((bindir / "data").lexically_normal());
        asset_store::add_search_path(ASSET_DIR);
        asset_store::load_assets(ASSET_DIR "/assets.json");

        push_layer(new RTLight());
    }

    ~GraphicsPlayground() {
    }
};

int main(int argc, char **argv) {
    WindowCreateInfo window_info = {
        .title  = "Milg",
        .width  = 1600,
        .height = 900,
    };

    GraphicsPlayground app(argc, argv, window_info);
    app.run();

    return 0;
}
