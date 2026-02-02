//
// Created by firef on 1/30/2026.
//

#include "../xr/xr_include.h"
#include "../util/log.h"
#include <stdbool.h>

#ifndef QCXR_VK_INIT_H
#define QCXR_VK_INIT_H

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    uint32_t queueFamilyIndex;
    VkQueue graphicsQueue;

    VkCommandPool commandPool;
    VkPipelineCache pipelineCache;

    VkPhysicalDeviceMemoryProperties cachedMemProps;
    VmaAllocator allocator;

    bool initialized;
} vk_info_t;

extern vk_info_t vkinfo;

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
bool initVulkan(XrInstance xrInstance, XrSystemId systemId);
void destroyVulkan();

#endif //QCXR_VK_INIT_H
