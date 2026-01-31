//
// Created by firef on 1/30/2026.
//

#ifndef QUESTCRAFT_RENDERER_TYPES_H
#define QUESTCRAFT_RENDERER_TYPES_H

#include "xr_include.h"

typedef struct {
    VkBuffer buffer;
    VmaAllocation allocation;
    uint32_t vertexCount;
    VkPipeline pipeline; // note: this is actually really bad architecturally, but I am way too lazy to fix it rn
} vk_model_t;

typedef struct {
    VkImage image;
    VmaAllocation allocation;
    VkImageView view;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
} vk_texture_t;

#endif //QUESTCRAFT_RENDERER_TYPES_H
