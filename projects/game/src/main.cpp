#include <milg/audio.hpp>
#include <milg/graphics.hpp>
#include <milg/graphics/map.hpp>
#include <milg/graphics/sprite_batch.hpp>
#include <milg/graphics/texture.hpp>
#include <milg/milg.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <cstdint>

using namespace milg;
using namespace milg::graphics;

class GraphicsLayer : public Layer {
public:
    std::shared_ptr<VulkanContext> context = nullptr;
    // This will hold whatever we render in the layer
    std::shared_ptr<Texture>     framebuffer  = nullptr;
    std::shared_ptr<SpriteBatch> sprite_batch = nullptr;
    std::shared_ptr<Map>         map;

    void on_attach() override {
        MILG_INFO("Initializing Graphics layer");

        context      = Application::get().context();
        auto &window = Application::get().window();

        TextureCreateInfo texture_info = {
            .format     = VK_FORMAT_R8G8B8A8_UNORM,
            .usage      = VK_IMAGE_USAGE_SAMPLED_BIT,
            .min_filter = VK_FILTER_NEAREST,
            .mag_filter = VK_FILTER_NEAREST,
        };

        this->map = AssetStore::load<Map>("maps/desert.tmj");

        this->framebuffer =
            Texture::create(context,
                            {.format = VK_FORMAT_R8G8B8A8_UNORM,
                             .usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
                            window->width(), window->height());

        // Capacity here is the maximum amount of sprites that can be drawn in one frame, more number
        // allocates more memory, but it's not that much to begin with
        this->sprite_batch = SpriteBatch::create(context, framebuffer->format(), 10000);
    }

    void on_update(float delta) override {
        // Acquire a command buffer from the application, it will be automatically submitted and disposed at the end of
        // the frame
        auto command_buffer = Application::get().acquire_command_buffer();

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        // Begin recording the command buffer and transition the framebuffer to a layout we can render to
        context->device_table().vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
        framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Construct an orthographic projection matrix that is the size of the framebuffer and the center is at (0, 0)
        float     half_width  = framebuffer->width() * 0.5f;
        float     half_height = framebuffer->height() * 0.5f;
        glm::mat4 mat         = glm::ortho(-half_width, half_width, -half_height, half_height, -1.0f, 1.0f);

        // Move the center of the projection matrix to the top left corner
        mat = glm::translate(mat, {-half_width, -half_height, 0.0f});

        // Reset the sprite batch, should be done once at the beginning of the frame
        sprite_batch->reset();
        sprite_batch->begin_batch(mat);

        auto       tile_size = this->map->get_tile_size();
        glm::ivec2 cursor;

        for (cursor.y = 0; cursor.y < framebuffer->height(); cursor.y += tile_size.y) {
            for (cursor.x = 0; cursor.x < framebuffer->width(); cursor.x += tile_size.x) {
                for (auto &tile : this->map->get_tiles(cursor)) {
                    sprite_batch->draw_sprite(tile->sprite, tile->tileset->get_texture());
                }
            }
        }

        // After drawing, build_batches should be called, this copies over data to the appropriate buffers
        sprite_batch->build_batches(command_buffer);

        // Prepare some Vulkan structs for rendering, only clearValue might be interesting here
        // as this will be the color the screen is cleared with
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
        const std::array<VkRenderingAttachmentInfo, 1> rendering_attachment_infos = {
            VkRenderingAttachmentInfo{
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
            },
        };

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

        // Prepare rendering state and let sprite batch execute the draw commands
        sprite_batch->render(command_buffer);

        context->device_table().vkCmdEndRendering(command_buffer);

        // After rendering, transition the framebuffer to a layout that can be used to copy from
        framebuffer->transition_layout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // And finally blit the framebuffer to the swapchain image that will be presented
        Application::get().swapchain()->blit_to_current_image(command_buffer, framebuffer->handle(),
                                                              {.width = extent.width, .height = extent.height});

        context->device_table().vkEndCommandBuffer(command_buffer);
    }

    void on_event(Event &event) override {
    }

    void on_detach() override {
        MILG_INFO("Tearing down Graphics layer");
    }
};

class Milgame : public Application {
public:
    Milgame(int argc, char **argv, const WindowCreateInfo &window_info) : Application(argc, argv, window_info) {
        auto bindir = std::filesystem::path(argv[0]).parent_path();

        AssetStore::add_search_path((bindir / "data").lexically_normal());
        AssetStore::add_search_path(ASSET_DIR);

        push_layer(new GraphicsLayer());
    }

    ~Milgame() {
    }
};

int main(int argc, char **argv) {
    return Milgame(argc, argv,
                   {
                       .title  = "Milg",
                       .width  = 1600,
                       .height = 900,
                   })
        .run();
}
