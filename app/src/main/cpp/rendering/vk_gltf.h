//
// Created by firef on 2/2/2026.
//

#ifndef QCXR_VK_GLTF_H
#define QCXR_VK_GLTF_H

#include "../xr/xr_include.h"
#include "vk_init.h"
#include "asset_buffer_read.h"
#include <android/asset_manager.h>

typedef struct {
    float pos[3];
    float normal[3];
    float uv[2];
} gltf_vertex_t;

typedef struct {
    allocated_buffer_t vertices;
    allocated_buffer_t indices;
    uint32_t index_count;
    uint32_t vertex_count;
    int material_index;
} gltf_mesh_t;

typedef struct {
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VkSampler sampler;
} vk_texture_t;

typedef struct {
    VkDescriptorSet* descriptor_sets;
} gltf_material_t;

typedef struct {
    gltf_mesh_t* meshes;
    uint32_t mesh_count;

    VkDescriptorPool descriptor_pool;
    gltf_material_t* materials;
    uint32_t material_count;

    vk_texture_t* textures;
    uint32_t texture_count;
} gltf_model_t;

bool model_load(asset_info_t* uploadInfo, gltf_model_t* out);
void model_draw(VkCommandBuffer cmdBuffer, uint32_t imageIndex, gltf_model_t* model);
void model_free(gltf_model_t* model);

#endif //QCXR_VK_GLTF_H
