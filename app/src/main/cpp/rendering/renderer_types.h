//
// Created by firef on 1/30/2026.
//

#ifndef QUESTCRAFT_RENDERER_TYPES_H
#define QUESTCRAFT_RENDERER_TYPES_H

#include "../xr/xr_include.h"
#include "../xr/xr_linear_algebra.h"
#include "vk_gltf.h"
#include <media/NdkImageReader.h>

#define SURFACE_WIDTH 2560
#define SURFACE_HEIGHT 1440

typedef struct {
    XrMatrix4x4f projectionViews[2]; // Max 2 views usually (apparently the fucking varjo has 4???)
    XrMatrix4x4f modelMatrix;
} UboViewData;

typedef struct {
    VkBuffer buffer;
    VmaAllocation allocation;
    uint32_t vertexCount;
} vk_model_t;

typedef struct {
    VkImage image;
    VmaAllocation allocation;
    VkImageView view;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkSampler sampler;
} vk_texture_t;

typedef struct {
    VkImage image;
    VmaAllocation allocation;
    VkImageView imageView;
} native_surface_texture_t;

typedef struct {
    VkPipelineLayout pipelineLayout;
    VkPipeline worldPipeline;
    VkPipeline linePipeline;
    VkPipeline blitPipeline;

    VkRenderPass renderPass;
    VkExtent2D depthSize;
    VkImage depthImage;
    VmaAllocation depthAlloc;
    VkImageView depthImageView;

    VkFramebuffer* framebuffers;
    uint32_t framebufferCount;
    VkImageView* swapchainImageViews;

    vk_model_t worldModel;
    vk_model_t targetRectModel;
    vk_model_t leftRay;
    vk_model_t rightRay;

    gltf_model_t worldModelGltf;

    vk_texture_t atlas;
    vk_texture_t light;

    AImageReader* surfaceReader;
    native_surface_texture_t* surfaceTextures;
    VkSampler surfaceSampler;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet* descriptorSets;

    VkDescriptorSetLayout set0Layout;
    VkDescriptorSetLayout set1Layout;

    VkDescriptorSetLayout gltfDescriptorSetLayout;
    VkPipelineLayout gltfPipelineLayout;
    VkPipeline gltfPipeline;

    VkBuffer uniformBuffer;
    VmaAllocation uniformAlloc;
    void* uniformMappedData;

    VkCommandBuffer* cmdBuffers;
    VkFence* renderFences;
} render_state_t;

#endif //QUESTCRAFT_RENDERER_TYPES_H
