//
// Created by firef on 2/2/2026.
//

#include "vk_gltf.h"
#define LOG_TAG __FILE_NAME__
#include "../util/log.h"

#define CGLTF_IMPLEMENTATION
#include "../third_party/cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"
#include "vk_init.h"
#include "renderer.h"
#include "asset_buffer_read.h"
#include "../xr/xr_init.h"
#include "ktx_texture.h"

vk_texture_t* missing;
gltf_texture_t* missing_mirror;

bool mesh_load(cgltf_data* data, cgltf_mesh* mesh, gltf_mesh_t* out) {
    cgltf_primitive* prim = &mesh->primitives[0];
    uint32_t vertex_count = (uint32_t)prim->attributes[0].data->count;
    uint32_t index_count = (uint32_t)prim->indices->count;

    gltf_vertex_t* vertices = malloc(sizeof(gltf_vertex_t) * vertex_count);
    uint32_t* indices = malloc(sizeof(uint32_t) * index_count);

    for (size_t i = 0; i < prim->attributes_count; i++) {
        cgltf_accessor* acc = prim->attributes[i].data;
        for (size_t j = 0; j < vertex_count; j++) {
            float v[4];
            cgltf_accessor_read_float(acc, j, v, cgltf_num_components(acc->type));

            if (prim->attributes[i].type == cgltf_attribute_type_position) {
                memcpy(vertices[j].pos, v, sizeof(float) * 3);
            } else if (prim->attributes[i].type == cgltf_attribute_type_normal) {
                memcpy(vertices[j].normal, v, sizeof(float) * 3);
            } else if (prim->attributes[i].type == cgltf_attribute_type_texcoord) {
                memcpy(vertices[j].uv, v, sizeof(float) * 2);
            }
        }
    }

    for (size_t i = 0; i < index_count; i++) {
        indices[i] = (uint32_t) cgltf_accessor_read_index(prim->indices, i);
    }

    upload_to_gpu(vertices, sizeof(gltf_vertex_t) * vertex_count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &out->vertices);
    upload_to_gpu(indices, sizeof(uint32_t) * index_count, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &out->indices);

    out->vertex_count = vertex_count;
    out->index_count = index_count;

    free(vertices);
    free(indices);

    return true;
}

static inline VkFilter cgltf_filter_to_vulkan(cgltf_filter_type filter) {
    switch (filter) {
        case cgltf_filter_type_nearest:
        case cgltf_filter_type_nearest_mipmap_nearest:
        case cgltf_filter_type_nearest_mipmap_linear:
            return VK_FILTER_NEAREST;
        case cgltf_filter_type_undefined:
        case cgltf_filter_type_linear:
        case cgltf_filter_type_linear_mipmap_nearest:
        case cgltf_filter_type_linear_mipmap_linear:
            return VK_FILTER_LINEAR;
    }
}

static inline VkSamplerAddressMode cgltf_wrap_to_vulkan(cgltf_wrap_mode wrap) {
    switch (wrap) {
        case cgltf_wrap_mode_repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case cgltf_wrap_mode_mirrored_repeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case cgltf_wrap_mode_clamp_to_edge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

bool texture_load(cgltf_texture* texture, gltf_texture_t* out) {
    uint8_t* pixel_data = NULL;
    int width, height, channels;
    size_t data_size = 0;

    if (texture->image->buffer_view) {
        uint8_t* base = (uint8_t*)texture->image->buffer_view->buffer->data + texture->image->buffer_view->offset;
        data_size = texture->image->buffer_view->size;
        pixel_data = stbi_load_from_memory(base, (int)data_size, &width, &height, &channels, 4);
    } else {
        LOGE("Texture is a file asset (we don't support non-embedded textures)");
        return false;
    }

    if (!pixel_data) {
        LOGE("Failed to load embedded texture for whatever reason");
        return false;
    }

    VkDeviceSize image_size = width * height * 4;
    VkImageCreateInfo image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB, // Albedo should be sRGB
            .extent = {width, height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo alloc_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateImage(vkinfo.allocator, &image_info, &alloc_info, &out->image, &out->allocation, NULL);

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = image_size,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo stagingAllocInfo = {
            .usage = VMA_MEMORY_USAGE_CPU_ONLY // Maps memory automatically for CPU access
    };

    vmaCreateBuffer(vkinfo.allocator, &bufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* mapped_data;
    vmaMapMemory(vkinfo.allocator, stagingAllocation, &mapped_data);
    memcpy(mapped_data, pixel_data, (size_t)image_size);
    vmaUnmapMemory(vkinfo.allocator, stagingAllocation);

    stbi_image_free(pixel_data);

    VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vkinfo.commandPool,
            .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(vkinfo.device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = out->image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);

    VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(cmd, stagingBuffer, out->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, NULL, 0, NULL, 1, &barrier);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
    };

    vkQueueSubmit(vkinfo.graphicsQueue, 1, &submitInfo, NULL);
    vkQueueWaitIdle(vkinfo.graphicsQueue);
    vkFreeCommandBuffers(vkinfo.device, vkinfo.commandPool, 1, &cmd);
    vmaDestroyBuffer(vkinfo.allocator, stagingBuffer, stagingAllocation);

    VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = out->image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCreateImageView(vkinfo.device, &view_info, NULL, &out->view);

    VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = cgltf_filter_to_vulkan(texture->sampler->mag_filter),
            .minFilter = cgltf_filter_to_vulkan(texture->sampler->min_filter),
            .addressModeU = cgltf_wrap_to_vulkan(texture->sampler->wrap_s),
            .addressModeV = cgltf_wrap_to_vulkan(texture->sampler->wrap_t),
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR // so the thing about this is that gltf encodes this the same way gl does which is in the mag and min filters but it doesn't make sense to
    };
    vkCreateSampler(vkinfo.device, &sampler_info, NULL, &out->sampler);

    LOGI("Created material texture with image: %p, view: %p, sampler: %p", out->image, out->view, out->sampler);
    return true;
}

bool material_load(cgltf_data* data, cgltf_material* material, gltf_material_t* out, gltf_model_t* model) { // this takes a full model pointer so it can retrieve the textures
    if (!(material->has_pbr_metallic_roughness)) {
        LOGE("We need a special fancy pipeline to render pipelines other than metallic roughness, so lets just only use metallic roughness now.");
        return false;
    }

    cgltf_pbr_metallic_roughness mat = material->pbr_metallic_roughness;
    int base_color_index = mat.base_color_texture.texture - data->textures;
    int metallic_roughness_index = mat.metallic_roughness_texture.texture - data->textures;

    gltf_texture_t base_color = mat.base_color_texture.texture ? model->textures[base_color_index] : *missing_mirror;
    gltf_texture_t metallic = mat.metallic_roughness_texture.texture ? model->textures[metallic_roughness_index] : *missing_mirror;

    uint32_t totalSets = xrinfo.renderTarget.swapchainImageCount * 2;

    out->descriptor_sets = malloc(totalSets * sizeof(VkDescriptorSet));
    VkDescriptorSetLayout* layouts = malloc(totalSets * sizeof(VkDescriptorSetLayout));
    for (uint32_t i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
        layouts[i * 2 + 0] = vk_rs.set0Layout;
        layouts[i * 2 + 1] = vk_rs.gltfDescriptorSetLayout;
    }

    VkDescriptorSetAllocateInfo setInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorSetCount = totalSets,
            .pSetLayouts = layouts,
            .descriptorPool = model->descriptor_pool
    };
    VkResult result = vkAllocateDescriptorSets(vkinfo.device, &setInfo, out->descriptor_sets);
    if (result != VK_SUCCESS) {
        LOGE("Failed to allocate descriptor sets! Error code: %d", result);
        return false;
    }

    VkDescriptorImageInfo colorInfo = {
            .sampler = base_color.sampler,
            .imageView = base_color.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorImageInfo metalInfo = {
            .sampler = metallic.sampler,
            .imageView = metallic.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    for (uint32_t i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
        VkDescriptorSet set0 = out->descriptor_sets[i * 2 + 0];
        VkDescriptorSet set1 = out->descriptor_sets[i * 2 + 1];

        VkDescriptorBufferInfo bufferInfo = {vk_rs.uniformBuffer, 0, sizeof(UboViewData) };

        VkDescriptorImageInfo imageInfos[2] = {
                colorInfo,
                metalInfo,
        };

        VkWriteDescriptorSet writes[3];
        writes[0] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set0, .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &bufferInfo };
        writes[1] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set1, .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &imageInfos[0] };
        writes[2] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set1, .dstBinding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &imageInfos[1] };

        vkUpdateDescriptorSets(vkinfo.device, 3, writes, 0, NULL);
    }

    return true;
}

void model_free(gltf_model_t* model) {
    if (!model) return;

    for (uint32_t i = 0; i < model->mesh_count; i++) {
        vmaDestroyBuffer(vkinfo.allocator, model->meshes[i].vertices.buffer, model->meshes[i].vertices.allocation);
        vmaDestroyBuffer(vkinfo.allocator, model->meshes[i].indices.buffer, model->meshes[i].indices.allocation);
    }
    free(model->meshes);

    for (uint32_t i = 0; i < model->texture_count; i++) {
        vkDestroySampler(vkinfo.device, model->textures[i].sampler, NULL);
        vkDestroyImageView(vkinfo.device, model->textures[i].view, NULL);
        vmaDestroyImage(vkinfo.allocator, model->textures[i].image, model->textures[i].allocation);
    }
    free(model->textures);

    if (model->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vkinfo.device, model->descriptor_pool, NULL);
    }
    if (model->materials) {
        free(model->materials);
    }

    model->mesh_count = 0;
    model->texture_count = 0;
}

bool model_load(asset_info_t* uploadInfo, gltf_model_t* out) {
    if (!missing) {
        missing = malloc(sizeof(vk_texture_t));
        loadKtx(&(asset_info_t){
                .path = "null.ktx",
                .assetManager = uploadInfo->assetManager
        }, missing);
        missing_mirror = malloc(sizeof(gltf_texture_t));
        missing_mirror->image = missing->image;
        missing_mirror->view = missing->view;
        missing_mirror->sampler = missing->sampler;
        // we're not gonna set allocation cuz this is a mirror
    }

    AAsset *asset = AAssetManager_open(uploadInfo->assetManager, uploadInfo->path, AASSET_MODE_STREAMING);
    if(asset == NULL) {
        LOGE("Failed to open asset \"%s\"", uploadInfo->path);
        return false;
    }

    const void* buf = AAsset_getBuffer(asset);
    size_t size = AAsset_getLength(asset);

    cgltf_options options = {
            .type = cgltf_file_type_invalid,
            .json_token_count = 0, // auto
            .memory = {
                    .alloc_func = cgltf_default_alloc,
                    .free_func = cgltf_default_free,
                    .user_data = NULL
            },
            .file = {
                    .read = cgltf_default_file_read,
                    .release = cgltf_default_file_release,
                    .user_data = NULL
            }
    };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, buf, size, &data);
    if (result != cgltf_result_success) {
        LOGE("Failed to load gltf file \"%s\"", uploadInfo->path);
        return false;
    }

    result = cgltf_load_buffers(&options, data, uploadInfo->path); // note: this won't work if it's a gltf file. need to implement file options properly.
    if (result != cgltf_result_success) {
        LOGE("Failed to load gltf buffers \"%s\"", uploadInfo->path);
        cgltf_free(data);
        return false;
    }

    out->meshes = NULL;
    out->textures = NULL;
    out->materials = NULL;
    out->descriptor_pool = VK_NULL_HANDLE;

    out->mesh_count = data->meshes_count;
    out->meshes = malloc(sizeof(gltf_mesh_t) * out->mesh_count);

    out->texture_count = data->textures_count;
    out->textures = malloc(sizeof(gltf_texture_t) * out->texture_count);

    out->material_count = data->materials_count;
    out->materials = malloc(sizeof(gltf_material_t) * out->material_count);

    for (uint32_t i = 0; i < data->textures_count; i++) {
        if (!texture_load(&data->textures[i], &out->textures[i])) goto fail;
    }

    VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, xrinfo.renderTarget.swapchainImageCount * data->materials_count},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, xrinfo.renderTarget.swapchainImageCount * 2 * data->materials_count}
    };

    VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = 2,
            .pPoolSizes = poolSizes,
            .maxSets = data->materials_count * 2 * xrinfo.renderTarget.swapchainImageCount
    };

    vkCreateDescriptorPool(vkinfo.device, &poolInfo, NULL, &out->descriptor_pool);

    for (uint32_t i = 0; i < data->materials_count; i++) {
        if (!material_load(data, &data->materials[i], &out->materials[i], out)) goto fail;
    }

    for (uint32_t i = 0; i < data->meshes_count; i++) {
        if (!mesh_load(data, &data->meshes[i], &out->meshes[i])) goto fail;

        if (data->meshes[i].primitives->material) {
            out->meshes[i].material_index = data->meshes[i].primitives->material - data->materials;
        } else out->meshes[i].material_index = -1;
    }

    LOGI("Loaded model with %zu meshes, %zu textures, and %zu materials.", data->meshes_count, data->textures_count, data->materials_count);
    cgltf_free(data);
    AAsset_close(asset);
    return true;
    fail:
    model_free(out);
    if (data) cgltf_free(data);
    if (asset) AAsset_close(asset);
    return false;
}

void model_draw(VkCommandBuffer cmdBuffer, uint32_t imageIndex, gltf_model_t* model) {
    for (int i = 0; i < model->mesh_count; i++) {
        gltf_mesh_t mesh = model->meshes[i];
        if (mesh.material_index != -1) {
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    vk_rs.gltfPipelineLayout, 1, 1,
                                    &model->materials[mesh.material_index].descriptor_sets[imageIndex * 2 + 1], 0, 0);
        }
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &mesh.vertices.buffer, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, mesh.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmdBuffer, mesh.index_count, 1, 0, 0, 0);
    }
}