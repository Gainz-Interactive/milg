#include "vk_context.hpp"
#include "logging.hpp"
#include "window.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

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

        auto context                           = std::shared_ptr<VulkanContext>(new VulkanContext());
        context->m_instance                    = instance;
        context->m_physical_device             = physical_device;
        context->m_device                      = device;
        context->m_device_table                = device_table;
        context->m_graphics_queue_family_index = queue_family_index;
        context->m_debug_messenger             = debug_messenger;

        return context;
    } // namespace milg

    VulkanContext::~VulkanContext() {
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

    VkDevice VulkanContext::device() const {
        return m_device;
    }

    const VolkDeviceTable &VulkanContext::device_table() const {
        return m_device_table;
    }

    uint32_t VulkanContext::graphics_queue_family_index() const {
        return m_graphics_queue_family_index;
    }
} // namespace milg
