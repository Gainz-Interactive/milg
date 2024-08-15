#include "vk_context.hpp"
#include "window.hpp"
#include <cstdint>
#include <milg.hpp>
#include <vulkan/vulkan_core.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <vector>

const std::vector<const char *> requested_instance_layers = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char *> requested_device_layers   = {"VK_LAYER_KHRONOS_validation"};

namespace milg {
    std::shared_ptr<VulkanContext> VulkanContext::create(const std::unique_ptr<Window> &window) {
        MILG_INFO("Creating Vulkan context");
        VK_CHECK(volkInitialize());

        const VkApplicationInfo app_info = {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "Milg",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "Milg",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_3,
        };

        std::vector<const char *> requested_instance_extensions = {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        };
        window->get_instance_extensions(requested_instance_extensions);

        auto debug_callback = [](VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
                                 VkDebugUtilsMessageTypeFlagsEXT             message_type,
                                 const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
            switch (message_severity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                MILG_INFO("{}", callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                MILG_WARN("{}", callback_data->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                MILG_ERROR("{}", callback_data->pMessage);
                break;
            default:
                break;
            }
            return VK_FALSE;
        };

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            .flags = 0,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
            .pUserData       = nullptr,
        };

        const VkInstanceCreateInfo instance_info = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = &debug_messenger_info,
            .flags                   = 0,
            .pApplicationInfo        = &app_info,
            .enabledLayerCount       = static_cast<uint32_t>(requested_instance_layers.size()),
            .ppEnabledLayerNames     = requested_instance_layers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requested_instance_extensions.size()),
            .ppEnabledExtensionNames = requested_instance_extensions.data(),
        };

        VkInstance instance;
        VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance));
        volkLoadInstanceOnly(instance);

        VkDebugUtilsMessengerEXT debug_messenger;
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_info, nullptr, &debug_messenger));

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
            MILG_CRITICAL("No suitable physical device found");
            return nullptr;
        }

        VkPhysicalDevice physical_device = preferred_device != VK_NULL_HANDLE ? preferred_device : fallback_device;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        MILG_INFO("Using physical device: {}", properties.deviceName);

        std::vector<const char *> requested_device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

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

        VkPhysicalDeviceFeatures features = {};

        VkPhysicalDeviceVulkan12Features vulkan_12_features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = nullptr,
        };
        vulkan_12_features.bufferDeviceAddress                          = VK_TRUE;
        vulkan_12_features.descriptorIndexing                           = VK_TRUE;
        vulkan_12_features.runtimeDescriptorArray                       = VK_TRUE;
        vulkan_12_features.descriptorBindingPartiallyBound              = VK_TRUE;
        vulkan_12_features.descriptorBindingVariableDescriptorCount     = VK_TRUE;
        vulkan_12_features.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
        vulkan_12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        vulkan_12_features.hostQueryReset                               = VK_TRUE;

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
            .enabledLayerCount       = static_cast<uint32_t>(requested_device_layers.size()),
            .ppEnabledLayerNames     = requested_device_layers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requested_device_extensions.size()),
            .ppEnabledExtensionNames = requested_device_extensions.data(),
            .pEnabledFeatures        = &features,
        };

        VkDevice device;
        VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device));
        volkLoadDevice(device);

        VolkDeviceTable device_table;
        volkLoadDeviceTable(&device_table, device);

        VkQueue graphics_queue;
        device_table.vkGetDeviceQueue(device, queue_family_index, 0, &graphics_queue);

        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

        const VmaVulkanFunctions vma_functions = {
            .vkGetInstanceProcAddr                   = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr                     = vkGetDeviceProcAddr,
            .vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory                        = device_table.vkAllocateMemory,
            .vkFreeMemory                            = device_table.vkFreeMemory,
            .vkMapMemory                             = device_table.vkMapMemory,
            .vkUnmapMemory                           = device_table.vkUnmapMemory,
            .vkFlushMappedMemoryRanges               = device_table.vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges          = device_table.vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory                      = device_table.vkBindBufferMemory,
            .vkBindImageMemory                       = device_table.vkBindImageMemory,
            .vkGetBufferMemoryRequirements           = device_table.vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements            = device_table.vkGetImageMemoryRequirements,
            .vkCreateBuffer                          = device_table.vkCreateBuffer,
            .vkDestroyBuffer                         = device_table.vkDestroyBuffer,
            .vkCreateImage                           = device_table.vkCreateImage,
            .vkDestroyImage                          = device_table.vkDestroyImage,
            .vkCmdCopyBuffer                         = device_table.vkCmdCopyBuffer,
            .vkGetBufferMemoryRequirements2KHR       = device_table.vkGetBufferMemoryRequirements2KHR,
            .vkGetImageMemoryRequirements2KHR        = device_table.vkGetImageMemoryRequirements2KHR,
            .vkBindBufferMemory2KHR                  = device_table.vkBindBufferMemory2KHR,
            .vkBindImageMemory2KHR                   = device_table.vkBindImageMemory2KHR,
            .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
            .vkGetDeviceBufferMemoryRequirements     = device_table.vkGetDeviceBufferMemoryRequirements,
            .vkGetDeviceImageMemoryRequirements      = device_table.vkGetDeviceImageMemoryRequirements,
        };

        const VmaAllocatorCreateInfo allocator_info = {
            .flags                          = 0,
            .physicalDevice                 = physical_device,
            .device                         = device,
            .preferredLargeHeapBlockSize    = 0,
            .pAllocationCallbacks           = nullptr,
            .pDeviceMemoryCallbacks         = nullptr,
            .pHeapSizeLimit                 = nullptr,
            .pVulkanFunctions               = &vma_functions,
            .instance                       = instance,
            .vulkanApiVersion               = VK_API_VERSION_1_3,
            .pTypeExternalMemoryHandleTypes = nullptr,
        };

        VmaAllocator allocator = VK_NULL_HANDLE;
        VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator));

        const VkCommandPoolCreateInfo command_pool_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queue_family_index,
        };

        VkCommandPool command_pool = VK_NULL_HANDLE;
        VK_CHECK(device_table.vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool));

        auto context                           = std::shared_ptr<VulkanContext>(new VulkanContext());
        context->m_instance                    = instance;
        context->m_physical_device             = physical_device;
        context->m_device                      = device;
        context->m_device_properties           = properties;
        context->m_device_table                = device_table;
        context->m_graphics_queue_family_index = queue_family_index;
        context->m_debug_messenger             = debug_messenger;
        context->m_graphics_queue              = graphics_queue;
        context->m_memory_properties           = memory_properties;
        context->m_allocator                   = allocator;
        context->m_command_pool                = command_pool;

        return context;
    }

    VulkanContext::~VulkanContext() {
        vmaDestroyAllocator(m_allocator);
        m_device_table.vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        m_device_table.vkDestroyDevice(m_device, nullptr);
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    VkInstance VulkanContext::instance() const {
        return m_instance;
    }

    VkPhysicalDevice VulkanContext::physical_device() const {
        return m_physical_device;
    }

    const VkPhysicalDeviceMemoryProperties &VulkanContext::memory_properties() const {
        return m_memory_properties;
    }

    const VkPhysicalDeviceProperties &VulkanContext::device_properties() const {
        return m_device_properties;
    }

    const VkPhysicalDeviceLimits &VulkanContext::device_limits() const {
        return m_device_properties.limits;
    }

    VkDevice VulkanContext::device() const {
        return m_device;
    }

    const VolkDeviceTable &VulkanContext::device_table() const {
        return m_device_table;
    }

    uint32_t VulkanContext::graphics_queue_family_index() const {
        return m_graphics_queue_family_index;
    }

    VkQueue VulkanContext::graphics_queue() const {
        return m_graphics_queue;
    }

    VmaAllocator VulkanContext::allocator() const {
        return m_allocator;
    }

    uint32_t VulkanContext::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) const {
        for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) &&
                (m_memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        return UINT32_MAX;
    }

    void VulkanContext::transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
                                                VkImageLayout new_layout) const {
        VkImageMemoryBarrier2 image_barrier = {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
            .oldLayout           = old_layout,
            .newLayout           = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = image,
            .subresourceRange    = {.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel   = 0,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1},

        };

        VkDependencyInfo dependency_info = {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers    = nullptr,
            .imageMemoryBarrierCount  = 1,
            .pImageMemoryBarriers     = &image_barrier,
        };

        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    }

    VkCommandBuffer VulkanContext::begin_single_time_commands() const {
        VkCommandBufferAllocateInfo allocate_info = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = m_command_pool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VK_CHECK(m_device_table.vkAllocateCommandBuffers(m_device, &allocate_info, &command_buffer));

        VkCommandBufferBeginInfo begin_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
        };

        VK_CHECK(m_device_table.vkBeginCommandBuffer(command_buffer, &begin_info));

        return command_buffer;
    }

    void VulkanContext::end_single_time_commands(VkCommandBuffer command_buffer) const {
        VK_CHECK(m_device_table.vkEndCommandBuffer(command_buffer));

        VkSubmitInfo submit_info = {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &command_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr,
        };

        VK_CHECK(m_device_table.vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
        VK_CHECK(m_device_table.vkQueueWaitIdle(m_graphics_queue));

        m_device_table.vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
    }
} // namespace milg
