#include "SDL_video.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#define VK_CHECK(func)                                                                                                 \
    do {                                                                                                               \
        VkResult result = func;                                                                                        \
        if (result != VK_SUCCESS) {                                                                                    \
            std::printf("%s:%d Vulkan error: \"%s\"", __FILE__, __LINE__, string_VkResult(result));                    \
        }                                                                                                              \
    } while (0)

SDL_Window *create_window() {
    std::printf("Creating SDL window\n");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL_Init Error: %s\n", SDL_GetError());
        return nullptr;
    }

    SDL_Window *window =
        SDL_CreateWindow("Milg", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN);
    if (window == nullptr) {
        std::printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return nullptr;
    }

    return window;
}

std::vector<const char *> get_required_device_extensions() {
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
}

std::vector<const char *> get_enabled_layers() {
    return {
        "VK_LAYER_KHRONOS_validation",
    };
}

std::vector<const char *> get_required_instance_extensions(SDL_Window *window) {
    uint32_t extension_count = 0u;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr)) {
        std::printf("SDL_Vulkan_GetInstanceExtensions Error: %s\n", SDL_GetError());
        return {};
    }

    std::vector<const char *> extensions(extension_count);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions.data())) {
        std::printf("SDL_Vulkan_GetInstanceExtensions Error: %s\n", SDL_GetError());
        return {};
    }

    return extensions;
}

VkInstance create_vulkan_instance(const std::vector<const char *> &extensions,
                                  const std::vector<const char *> &layers = {}) {
    std::printf("Creating Vulkan instance\n");
    VK_CHECK(volkInitialize());

    uint32_t version = volkGetInstanceVersion();
    std::printf("Vulkan version: %d.%d.%d\n", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version),
                VK_VERSION_PATCH(version));

    const VkApplicationInfo app_info = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = nullptr,
        .pApplicationName   = "Milg",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "Milg",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_3,
    };

    const VkInstanceCreateInfo instance_info = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .pApplicationInfo        = &app_info,
        .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance));

    volkLoadInstanceOnly(instance);

    return instance;
}

VkPhysicalDevice pick_physical_device(VkInstance instance) {
    std::printf("Picking Vulkan physical device\n");

    uint32_t physical_device_count = 0u;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

    VkPhysicalDevice preferred_device = VK_NULL_HANDLE;
    VkPhysicalDevice fallback_device  = VK_NULL_HANDLE;

    for (const auto &device : physical_devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            preferred_device = device;
            break;
        }

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            fallback_device = device;
        }
    }

    if (preferred_device == VK_NULL_HANDLE && fallback_device == VK_NULL_HANDLE) {
        std::printf("No suitable physical device found\n");
    }

    return preferred_device != VK_NULL_HANDLE ? preferred_device : fallback_device;
}

std::tuple<VkDevice, uint32_t> create_device(VkPhysicalDevice                 physical_device,
                                             const std::vector<const char *> &extensions,
                                             const std::vector<const char *> &layers = {}) {
    std::printf("Creating Vulkan device\n");

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    uint32_t queue_family_count = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    uint32_t queue_family_index = 0u;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_index = i;
            break;
        }
    }

    float                         queue_priority = 1.0f;
    const VkDeviceQueueCreateInfo queue_info     = {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = queue_family_index,
            .queueCount       = 1,
            .pQueuePriorities = &queue_priority,
    };

    VkPhysicalDeviceVulkan12Features vulkan_12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
    };
    vulkan_12_features.bufferDeviceAddress = VK_TRUE;
    vulkan_12_features.descriptorIndexing  = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan_13_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vulkan_12_features,
    };
    vulkan_13_features.synchronization2 = VK_TRUE;
    vulkan_13_features.dynamicRendering = VK_TRUE;

    const VkDeviceCreateInfo device_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &vulkan_13_features,
        .flags                   = 0,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &queue_info,
        .enabledLayerCount       = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames     = layers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures        = &features,
    };

    VkDevice device;
    VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device));

    volkLoadDevice(device);

    return {device, queue_family_index};
}

VkSurfaceKHR create_surface(VkInstance instance, SDL_Window *window) {
    std::printf("Creating Vulkan surface\n");

    VkSurfaceKHR surface;
    SDL_bool     result = SDL_Vulkan_CreateSurface(window, instance, &surface);
    if (result == SDL_FALSE) {
        std::printf("Failed to create Vulkan surface\n");
        return VK_NULL_HANDLE;
    }

    return surface;
}

std::tuple<VkSwapchainKHR, VkFormat> create_swapchain(VkSurfaceKHR surface, VkPhysicalDevice physical_device,
                                                      VkDevice device, VolkDeviceTable &device_table) {
    std::printf("Creating Vulkan swapchain\n");

    std::printf("Picking swapchain surface format\n");
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities));

    uint32_t format_count = 0u;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data()));

    VkFormat surface_format = VK_FORMAT_UNDEFINED;
    if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        surface_format = VK_FORMAT_R8G8B8A8_UNORM;
    } else {
        for (const auto &format : formats) {
            if (format.format == VK_FORMAT_R8G8B8A8_UNORM) {
                surface_format = format.format;
                break;
            }
        }

        if (surface_format == VK_FORMAT_UNDEFINED) {
            surface_format = formats[0].format;
        }
    }

    std::printf("Selected surface format: %s\n", string_VkFormat(surface_format));

    const VkSwapchainCreateInfoKHR swapchain_info = {
        .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext                 = nullptr,
        .flags                 = 0,
        .surface               = surface,
        .minImageCount         = surface_capabilities.minImageCount,
        .imageFormat           = surface_format,
        .imageColorSpace       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent           = {800, 600},
        .imageArrayLayers      = 1,
        .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .preTransform          = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = VK_PRESENT_MODE_FIFO_KHR,
        .clipped               = VK_TRUE,
        .oldSwapchain          = VK_NULL_HANDLE,
    };

    VkSwapchainKHR swapchain;
    VK_CHECK(device_table.vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain));

    return {swapchain, surface_format};
}

std::tuple<std::vector<VkImage>, std::vector<VkImageView>> get_swapchain_images(VkSwapchainKHR   swapchain,
                                                                                VkFormat         surface_format,
                                                                                VkDevice         device,
                                                                                VolkDeviceTable &device_table) {
    uint32_t image_count = 0u;
    VK_CHECK(device_table.vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr));

    std::vector<VkImage> swapchain_images(image_count);
    VK_CHECK(device_table.vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data()));

    std::vector<VkImageView> swapchain_image_views(image_count);
    for (size_t i = 0; i < image_count; ++i) {
        const VkImageViewCreateInfo image_view_info = {.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                       .pNext    = nullptr,
                                                       .flags    = 0,
                                                       .image    = swapchain_images[i],
                                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                       .format   = surface_format,
                                                       .components =
                                                           {
                                                               .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                               .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                               .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                               .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                           },
                                                       .subresourceRange = {
                                                           .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                                           .baseMipLevel   = 0,
                                                           .levelCount     = 1,
                                                           .baseArrayLayer = 0,
                                                           .layerCount     = 1,
                                                       }};

        VK_CHECK(device_table.vkCreateImageView(device, &image_view_info, nullptr, &swapchain_image_views[i]));
    }

    return {swapchain_images, swapchain_image_views};
}

std::tuple<VkCommandPool, std::vector<VkCommandBuffer>>
create_command_structures(VkDevice device, VolkDeviceTable &device_table, size_t command_buffer_count) {
    std::printf("Creating Vulkan command pool\n");

    const VkCommandPoolCreateInfo command_pool_info = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = 0,
    };

    VkCommandPool command_pool;
    VK_CHECK(device_table.vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool));

    std::printf("Allocating Vulkan command buffers\n");

    const VkCommandBufferAllocateInfo command_buffer_info = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(command_buffer_count),
    };

    std::vector<VkCommandBuffer> command_buffers(command_buffer_count);
    VK_CHECK(device_table.vkAllocateCommandBuffers(device, &command_buffer_info, command_buffers.data()));

    return {command_pool, command_buffers};
}

std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>> create_semaphores(VkDevice         device,
                                                                                 VolkDeviceTable &device_table) {
    std::printf("Creating Vulkan semaphores\n");

    std::vector<VkSemaphore> image_available_semaphores(2);
    std::vector<VkSemaphore> render_finished_semaphores(2);

    for (size_t i = 0; i < 2; ++i) {
        const VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };

        VK_CHECK(device_table.vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]));
        VK_CHECK(device_table.vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]));
    }

    return {image_available_semaphores, render_finished_semaphores};
}

std::vector<VkFence> create_frame_fences(VkDevice device, VolkDeviceTable &device_table) {
    std::printf("Creating Vulkan fences\n");

    std::vector<VkFence> frame_fences(2);
    for (size_t i = 0; i < 2; ++i) {
        const VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        VK_CHECK(device_table.vkCreateFence(device, &fence_info, nullptr, &frame_fences[i]));
    }

    return frame_fences;
}

void transition_image(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkCommandBuffer command_buffer,
                      VkDevice device, VolkDeviceTable &device_table) {
    const VkImageMemoryBarrier barrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout           = old_layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange =
            {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    };

    const VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    const VkPipelineStageFlags target_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    device_table.vkCmdPipelineBarrier(command_buffer, source_stage, target_stage, 0, 0, nullptr, 0, nullptr, 1,
                                      &barrier);
}

VkShaderModule load_shader_module(const std::filesystem::path &path, VkDevice device, VolkDeviceTable &device_table) {
    std::printf("Loading shader module from file: %s\n", path.c_str());

    std::ifstream file(path);
    if (!file.is_open()) {
        std::printf("Failed to open file\n");
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
    VK_CHECK(device_table.vkCreateShaderModule(device, &shader_module_info, nullptr, &shader_module));

    return shader_module;
}

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice device, VolkDeviceTable &device_table) {
    std::printf("Creating Vulkan descriptor set layout\n");

    const VkDescriptorSetLayoutBinding layout_binding = {
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = nullptr,
    };

    const VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = nullptr,
        .flags        = 0,
        .bindingCount = 1,
        .pBindings    = &layout_binding,
    };

    VkDescriptorSetLayout descriptor_set_layout;
    VK_CHECK(device_table.vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &descriptor_set_layout));

    return descriptor_set_layout;
}

VkPipelineLayout create_pipeline_layout(VkDevice device, VolkDeviceTable &device_table) {
    std::printf("Creating Vulkan pipeline layout\n");

    const VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size       = sizeof(float) * 4,
    };

    const VkPipelineLayoutCreateInfo layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &push_constant_range,
    };

    VkPipelineLayout pipeline_layout;
    VK_CHECK(device_table.vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout));

    return pipeline_layout;
}

VkPipeline create_graphics_pipeline(VkShaderModule vertex_shader_module, VkShaderModule fragment_shader_module,
                                    VkPipelineLayout pipeline_layout, VkFormat surface_format, VkDevice device,
                                    VolkDeviceTable &device_table) {
    const VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
        .lineWidth               = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = 0.0f,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };

    const VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment,
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    const std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = 0,
        .dynamicStateCount = dynamic_states.size(),
        .pDynamicStates    = dynamic_states.data(),
    };

    const VkPipelineViewportStateCreateInfo viewport_state = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = nullptr,
        .scissorCount  = 1,
        .pScissors     = nullptr,
    };

    const VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = vertex_shader_module,
            .pName               = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = fragment_shader_module,
            .pName               = "main",
            .pSpecializationInfo = nullptr,
        },
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineRenderingCreateInfo dynamic_rendering_info = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .pNext                   = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &surface_format,
        .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &dynamic_rendering_info,
        .flags               = 0,
        .stageCount          = 2,
        .pStages             = shader_stages,
        .pVertexInputState   = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewport_state,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState   = &multisample_info,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &color_blend_info,
        .pDynamicState       = &dynamic_state,
        .layout              = pipeline_layout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };

    VkPipeline pipeline;
    VK_CHECK(device_table.vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

    return pipeline;
}

VkDescriptorPool initialize_imgui_context(SDL_Window *window, VkInstance instance, VkPhysicalDevice physical_device,
                                          uint32_t queue_family_index, VkQueue queue, VkDevice device,
                                          VkFormat surface_format, VolkDeviceTable &device_table) {
    ImGuiContext *imgui_context = ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui_ImplSDL2_InitForVulkan(window);

    const VkDescriptorPoolSize imgui_pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    const VkDescriptorPoolCreateInfo pool_info = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 1000,
        .poolSizeCount = (uint32_t)std::size(imgui_pool_sizes),
        .pPoolSizes    = imgui_pool_sizes,
    };

    VkDescriptorPool imgui_descriptor_pool;
    VK_CHECK(device_table.vkCreateDescriptorPool(device, &pool_info, nullptr, &imgui_descriptor_pool));

    ImGui_ImplVulkan_InitInfo imgui_init_info = {
        .Instance            = instance,
        .PhysicalDevice      = physical_device,
        .Device              = device,
        .QueueFamily         = queue_family_index,
        .Queue               = queue,
        .DescriptorPool      = imgui_descriptor_pool,
        .MinImageCount       = 2,
        .ImageCount          = 2,
        .MSAASamples         = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache       = VK_NULL_HANDLE,
        .Subpass             = 0,
        .UseDynamicRendering = VK_TRUE,
        .PipelineRenderingCreateInfo =
            {
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .pNext                   = nullptr,
                .viewMask                = 0,
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &surface_format,
                .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
                .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
            },
    };
    ImGui_ImplVulkan_Init(&imgui_init_info);
    ImGui_ImplVulkan_CreateFontsTexture();

    return imgui_descriptor_pool;
}

int main() {
    SDL_Window *window = create_window();
    if (window == nullptr) {
        std::printf("Failed to create window\n");
        return 1;
    }

    std::vector<const char *> enabled_layers = get_enabled_layers();
    std::printf("Enabled layers:\n");
    for (const auto &layer : enabled_layers) {
        std::printf("\t%s\n", layer);
    }

    std::vector<const char *> instance_extensions = get_required_instance_extensions(window);
    std::printf("Required instance extensions:\n");
    for (const auto &extension : instance_extensions) {
        std::printf("\t%s\n", extension);
    }

    VkInstance       instance        = create_vulkan_instance(instance_extensions, enabled_layers);
    VkPhysicalDevice physical_device = pick_physical_device(instance);

    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
    std::printf("Selected Physical device: %s\n", physical_device_properties.deviceName);

    std::vector<const char *> enabled_device_extensions = get_required_device_extensions();
    std::printf("Required device extensions:\n");
    for (const auto &extension : enabled_device_extensions) {
        std::printf("\t%s\n", extension);
    }

    auto [device, queue_family_index] = create_device(physical_device, enabled_device_extensions, enabled_layers);
    VolkDeviceTable device_table;
    volkLoadDeviceTable(&device_table, device);
    VkSurfaceKHR surface             = create_surface(instance, window);
    auto [swapchain, surface_format] = create_swapchain(surface, physical_device, device, device_table);
    auto [swapchain_images, swapchain_image_views] =
        get_swapchain_images(swapchain, surface_format, device, device_table);

    VkQueue graphics_queue;
    device_table.vkGetDeviceQueue(device, queue_family_index, 0, &graphics_queue);

    auto [command_pool, command_buffers] = create_command_structures(device, device_table, swapchain_images.size());
    auto [image_available_semaphores, render_finished_semaphores] = create_semaphores(device, device_table);
    std::vector<VkFence> frame_fences                             = create_frame_fences(device, device_table);

    VkShaderModule vertex_shader_module   = load_shader_module("data/shaders/triangle.vert.spv", device, device_table);
    VkShaderModule fragment_shader_module = load_shader_module("data/shaders/triangle.frag.spv", device, device_table);
    VkPipelineLayout pipeline_layout      = create_pipeline_layout(device, device_table);
    VkPipeline       pipeline = create_graphics_pipeline(vertex_shader_module, fragment_shader_module, pipeline_layout,
                                                         surface_format, device, device_table);

    VkDescriptorPool imgui_descriptor_pool = initialize_imgui_context(
        window, instance, physical_device, queue_family_index, graphics_queue, device, surface_format, device_table);

    struct {
        float tint[3] = {1.0f, 1.0f, 1.0f};
        float size    = 1.0f;
    } push_constants;

    uint32_t frame_index = 0;
    bool     running     = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Tryngle");
        ImGui::ColorEdit3("Tint", push_constants.tint);
        ImGui::SliderFloat("Size", &push_constants.size, 0.1f, 1.0f);
        ImGui::End();

        ImGui::Render();

        device_table.vkWaitForFences(device, 1, &frame_fences[frame_index], VK_TRUE, UINT64_MAX);
        device_table.vkResetFences(device, 1, &frame_fences[frame_index]);

        uint32_t image_index;
        device_table.vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_semaphores[frame_index],
                                           VK_NULL_HANDLE, &image_index);

        const VkCommandBufferBeginInfo command_buffer_begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        device_table.vkBeginCommandBuffer(command_buffers[image_index], &command_buffer_begin_info);

        transition_image(swapchain_images[image_index], VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, command_buffers[image_index], device, device_table);

        const VkRenderingAttachmentInfo rendering_attachment_info = {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = swapchain_image_views[image_index],
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
            .renderArea           = {{0, 0}, {800, 600}},
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &rendering_attachment_info,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr,
        };
        device_table.vkCmdBeginRendering(command_buffers[image_index], &rendering_info);

        const VkRect2D   scissor  = {{0, 0}, {800, 600}};
        const VkViewport viewport = {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = 800.0f,
            .height   = 600.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        device_table.vkCmdBindPipeline(command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        device_table.vkCmdPushConstants(command_buffers[image_index], pipeline_layout,
                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                        sizeof(push_constants), &push_constants);
        device_table.vkCmdSetViewport(command_buffers[image_index], 0, 1, &viewport);
        device_table.vkCmdSetScissor(command_buffers[image_index], 0, 1, &scissor);

        device_table.vkCmdDraw(command_buffers[image_index], 3, 1, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffers[image_index]);

        device_table.vkCmdEndRendering(command_buffers[image_index]);

        transition_image(swapchain_images[image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, command_buffers[image_index], device, device_table);

        device_table.vkEndCommandBuffer(command_buffers[image_index]);

        const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo         submit_info         = {
                            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .pNext                = nullptr,
                            .waitSemaphoreCount   = 1,
                            .pWaitSemaphores      = &image_available_semaphores[frame_index],
                            .pWaitDstStageMask    = &wait_dst_stage_mask,
                            .commandBufferCount   = 1,
                            .pCommandBuffers      = &command_buffers[image_index],
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores    = &render_finished_semaphores[frame_index],
        };

        device_table.vkQueueSubmit(graphics_queue, 1, &submit_info, frame_fences[frame_index]);

        const VkPresentInfoKHR present_info = {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &render_finished_semaphores[frame_index],
            .swapchainCount     = 1,
            .pSwapchains        = &swapchain,
            .pImageIndices      = &image_index,
            .pResults           = nullptr,
        };

        device_table.vkQueuePresentKHR(graphics_queue, &present_info);

        frame_index = (frame_index + 1) % 2;
    }

    device_table.vkDeviceWaitIdle(device);

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device, imgui_descriptor_pool, nullptr);
    vkDestroyShaderModule(device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    for (auto fence : frame_fences) {
        vkDestroyFence(device, fence, nullptr);
    }
    for (auto semaphore : render_finished_semaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    for (auto semaphore : image_available_semaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    vkDestroyCommandPool(device, command_pool, nullptr);
    for (auto image_view : swapchain_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
