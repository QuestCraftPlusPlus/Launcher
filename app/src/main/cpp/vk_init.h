//
// Created by firef on 1/30/2026.
//

#ifndef QUESTCRAFT_VK_INIT_H
#define QUESTCRAFT_VK_INIT_H

#include "xr_include.h"
#include <stdbool.h>

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

bool initVulkan(XrInstance xrInstance, XrSystemId systemId);
void destroyVulkan();

#endif //QUESTCRAFT_VK_INIT_H
