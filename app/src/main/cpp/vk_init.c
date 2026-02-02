//
// Created by firef on 1/30/2026.
//

#include <malloc.h>
#include <signal.h>
#include "vk_init.h"

#define LOG_TAG __FILE_NAME__
#include "log.h"

vk_info_t vkinfo = {0};

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vkinfo.physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOGE("Failed to find suitable memory type!");
    return 0;
}

void destroyVulkan() {
    if (!vkinfo.initialized) return;
    LOGI("destroying vulkan");
    if (vkinfo.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vkinfo.device);

        if (vkinfo.pipelineCache != NULL) {
            vkDestroyPipelineCache(vkinfo.device, vkinfo.pipelineCache, NULL);
            vkinfo.pipelineCache = NULL;
        }

        if (vkinfo.allocator != NULL) {
            vmaDestroyAllocator(vkinfo.allocator);
            vkinfo.allocator = NULL;
        }

        if (vkinfo.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vkinfo.device, vkinfo.commandPool, NULL);
            vkinfo.commandPool = VK_NULL_HANDLE;
        }

        vkDestroyDevice(vkinfo.device, NULL);
        vkinfo.device = VK_NULL_HANDLE;
    }
    if (vkinfo.instance != VK_NULL_HANDLE) {
        if (vkinfo.debugMessenger != VK_NULL_HANDLE) {
            PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkinfo.instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != NULL) {
                func(vkinfo.instance, vkinfo.debugMessenger, NULL);
            }
        }

        vkDestroyInstance(vkinfo.instance, NULL);
        vkinfo.instance = VK_NULL_HANDLE;
    }
    vkinfo.initialized = false;
    LOGI("Vulkan resources were cleaned up.");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        __android_log_print(ANDROID_LOG_ERROR, "QuestCraft Validation", "Vulkan Validation Error: %s", pCallbackData->pMessage);
        raise(SIGTRAP);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        __android_log_print(ANDROID_LOG_WARN, "QuestCraft Validation", "Vulkan Validation Warning: %s", pCallbackData->pMessage);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "QuestCraft Validation", "Vulkan Validation Warning: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

bool initVulkan(XrInstance xrInstance, XrSystemId systemId) {
    PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR;
    PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR;
    PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR;

    XR_FAILRETURN(xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR", (PFN_xrVoidFunction*)&xrCreateVulkanInstanceKHR), false);
    XR_FAILRETURN(xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR", (PFN_xrVoidFunction*)&xrCreateVulkanDeviceKHR), false);
    XR_FAILRETURN(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDevice2KHR), false);

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
    };

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    VkExtensionProperties* availableExtensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * extensionCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, availableExtensions);

    LOGI("Available Vulkan Instance Extensions (%d):\n", extensionCount);
    for (uint32_t i = 0; i < extensionCount; i++) {
        LOGI("\t- %s (v%d)\n", availableExtensions[i].extensionName, availableExtensions[i].specVersion);
    }
    free(availableExtensions);

    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    const char* extensions[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };

    VkInstanceCreateInfo vkInstanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    vkInstanceCreateInfo.pNext = &debugCreateInfo;
    vkInstanceCreateInfo.enabledLayerCount = 1;
    vkInstanceCreateInfo.ppEnabledLayerNames = layers;
    vkInstanceCreateInfo.enabledExtensionCount = 2;
    vkInstanceCreateInfo.ppEnabledExtensionNames = extensions;
    vkInstanceCreateInfo.pApplicationInfo = &(VkApplicationInfo){
            .apiVersion = VK_API_VERSION_1_1, // supposedly the quest supports up to 1.3, but I've had mixed results. 1.1 has multiview support as a requirement though so we're sticking with that for now.
            .applicationVersion = VK_MAKE_API_VERSION(0,0,0,0),
            .engineVersion = VK_MAKE_API_VERSION(0,0,0,0),
            .pEngineName = "VkXrEngine",
            .pApplicationName = "QuestCraft"
    };
    XrVulkanInstanceCreateInfoKHR xrVkInstanceCreateInfo = { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
    xrVkInstanceCreateInfo.systemId = systemId;
    xrVkInstanceCreateInfo.pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) &vkGetInstanceProcAddr;
    xrVkInstanceCreateInfo.vulkanCreateInfo = &vkInstanceCreateInfo;
    xrVkInstanceCreateInfo.vulkanAllocator = NULL;

    VkResult vkResult;
    XR_FAILRETURN(xrCreateVulkanInstanceKHR(xrInstance, &xrVkInstanceCreateInfo, &vkinfo.instance, &vkResult), false);
    VK_FAILRETURN(vkResult, false);

    PFN_vkCreateDebugUtilsMessengerEXT createFunc = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkinfo.instance, "vkCreateDebugUtilsMessengerEXT");
    if (createFunc != NULL) {
        createFunc(vkinfo.instance, &debugCreateInfo, NULL, &vkinfo.debugMessenger);
    }

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo = {XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceGetInfo.systemId = systemId;
    deviceGetInfo.vulkanInstance = vkinfo.instance;
    XR_FAILRETURN(xrGetVulkanGraphicsDevice2KHR(xrInstance, &deviceGetInfo, &vkinfo.physicalDevice), false);

    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(vkinfo.physicalDevice, &queueCount, NULL);
    VkQueueFamilyProperties* queueProps = calloc(queueCount, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(vkinfo.physicalDevice, &queueCount, queueProps);
    uint32_t graphicsQueueFamilyIndex = -1;
    for (uint32_t i = 0; i < queueCount; i++) {
        if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamilyIndex = i;
            break;
        }
    }
    if (graphicsQueueFamilyIndex == -1) {
        LOGE("Couldn't find a valid graphics queue family!");
        return false;
    }

    float priorities = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.pQueuePriorities = &priorities;

    VkPhysicalDeviceVulkan11Features features11 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .samplerYcbcrConversion = VK_TRUE,
            .multiview = VK_TRUE,
    };

    VkDeviceCreateInfo vkDeviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    vkDeviceInfo.pQueueCreateInfos = &queueCreateInfo;
    vkDeviceInfo.queueCreateInfoCount = 1;
    vkDeviceInfo.pNext = &features11;

    XrVulkanDeviceCreateInfoKHR xrVkDeviceInfo = {XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrVkDeviceInfo.systemId = systemId;
    xrVkDeviceInfo.vulkanAllocator = NULL;
    xrVkDeviceInfo.pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) &vkGetInstanceProcAddr;
    xrVkDeviceInfo.vulkanPhysicalDevice = vkinfo.physicalDevice;
    xrVkDeviceInfo.vulkanCreateInfo = &vkDeviceInfo;

    XR_FAILRETURN(xrCreateVulkanDeviceKHR(xrInstance, &xrVkDeviceInfo, &vkinfo.device, &vkResult), false);
    VK_FAILRETURN(vkResult, false);

    vkinfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    vkGetDeviceQueue(vkinfo.device, graphicsQueueFamilyIndex, 0, &vkinfo.graphicsQueue);

    VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = vkinfo.queueFamilyIndex
    };

    VK_FAILRETURN(vkCreateCommandPool(vkinfo.device, &poolInfo, NULL, &vkinfo.commandPool), false);

    VmaVulkanFunctions vkFuncs = {0};
    vkFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vkFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo = {0};
    allocatorInfo.instance = vkinfo.instance;
    allocatorInfo.physicalDevice = vkinfo.physicalDevice;
    allocatorInfo.device = vkinfo.device;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
    allocatorInfo.pVulkanFunctions = &vkFuncs;

    VK_FAILRETURN(vmaCreateAllocator(&allocatorInfo, &vkinfo.allocator), false);
    vkGetPhysicalDeviceMemoryProperties(vkinfo.physicalDevice, &vkinfo.cachedMemProps);

    LOGI("We got us a full ass vulkan instance");

    VkPipelineCacheCreateInfo cacheCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    VK_FAILRETURN(vkCreatePipelineCache(vkinfo.device, &cacheCreateInfo, NULL, &vkinfo.pipelineCache), false);

    vkinfo.initialized = true;
    return true;
}