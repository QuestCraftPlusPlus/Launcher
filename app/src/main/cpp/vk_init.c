//
// Created by firef on 1/30/2026.
//

#include <malloc.h>
#include "vk_init.h"

#define LOG_TAG __FILE_NAME__
#include "log.h"

vk_info_t vkinfo = {0};

void destroyVulkan() {
    if (!vkinfo.initialized) return;
    if (vkinfo.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vkinfo.device);

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
        vkDestroyInstance(vkinfo.instance, NULL);
        vkinfo.instance = VK_NULL_HANDLE;
    }
    vkinfo.initialized = false;
    LOGI("Vulkan resources were cleaned up.");
}

bool initVulkan(XrInstance xrInstance, XrSystemId systemId) {

    PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR;
    PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR;
    PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR;

    XR_FAILRETURN(xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR", (PFN_xrVoidFunction*)&xrCreateVulkanInstanceKHR), false);
    XR_FAILRETURN(xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR", (PFN_xrVoidFunction*)&xrCreateVulkanDeviceKHR), false);
    XR_FAILRETURN(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR", (PFN_xrVoidFunction*)&xrGetVulkanGraphicsDevice2KHR), false);

    VkInstanceCreateInfo vkInstanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    vkInstanceCreateInfo.pApplicationInfo = &(VkApplicationInfo){
            .apiVersion = VK_API_VERSION_1_1,
            .applicationVersion = VK_MAKE_API_VERSION(0,0,0,0),
            .engineVersion = VK_MAKE_API_VERSION(0,0,0,0),
            .pEngineName = "VkXrEngine",
            .pApplicationName = "QuestCraft"
    };
    XrVulkanInstanceCreateInfoKHR xrVkInstanceCreateInfo = { XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR };
    xrVkInstanceCreateInfo.systemId = systemId;
    xrVkInstanceCreateInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
    xrVkInstanceCreateInfo.vulkanCreateInfo = &vkInstanceCreateInfo;
    xrVkInstanceCreateInfo.vulkanAllocator = NULL;

    VkResult vkResult;
    XR_FAILRETURN(xrCreateVulkanInstanceKHR(xrInstance, &xrVkInstanceCreateInfo, &vkinfo.instance, &vkResult), false);
    VK_FAILRETURN(vkResult, false);

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
    VkDeviceCreateInfo vkDeviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    vkDeviceInfo.pQueueCreateInfos = &queueCreateInfo;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
            .multiview = VK_TRUE
    };
    vkDeviceInfo.pNext = &multiviewFeatures;

    XrVulkanDeviceCreateInfoKHR xrVkDeviceInfo = {XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrVkDeviceInfo.systemId = systemId;
    xrVkDeviceInfo.vulkanAllocator = NULL;
    xrVkDeviceInfo.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
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

    vkinfo.initialized = true;
    return true;
}