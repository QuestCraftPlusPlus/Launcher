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

typedef struct {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
} allocated_buffer_t;

extern vk_info_t vkinfo;

bool upload_to_gpu(void* data, size_t size, VkBufferUsageFlagBits usage, allocated_buffer_t* buffer_out);
uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
bool initVulkan(XrInstance xrInstance, XrSystemId systemId);
void destroyVulkan();

#endif //QCXR_VK_INIT_H
