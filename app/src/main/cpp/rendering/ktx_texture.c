//
// Created by maks on 05.12.2024.
//

// Don't look too closely at this file.

#include "ktx_texture.h"
#include "vk_init.h"
#include "ktx.h"
#include "ktxvulkan.h"
#include <stdlib.h>
#include <math.h>

#define LOG_TAG __FILE_NAME__
#include "../util/log.h"

#define STB_DS_IMPLEMENTATION
#include "../third_party/stb_ds.h"

typedef struct {
    VmaAllocation allocation;
    VkDeviceSize mapSize;
} AllocationInfo;

typedef struct {
    uint64_t key;
    AllocationInfo value;
} AllocationMap;

AllocationMap* gAllocationTable = NULL;
uint64_t gNextId = 1;
uint64_t KtxVmaAlloc(VkMemoryAllocateInfo* allocInfo, VkMemoryRequirements* memReq, uint64_t* pageCount) {
    VmaAllocationCreateInfo vmaAllocInfo = {0};
    if ((vkinfo.cachedMemProps.memoryTypes[allocInfo->memoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ||
        (vkinfo.cachedMemProps.memoryTypes[allocInfo->memoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    } else {
        vmaAllocInfo.flags = VMA_MEMORY_USAGE_GPU_ONLY;
    }
    vmaAllocInfo.memoryTypeBits = memReq->memoryTypeBits;

    VmaAllocation allocation;
    VkResult result = vmaAllocateMemory(vkinfo.allocator, memReq, &vmaAllocInfo, &allocation, NULL);
    if (result != VK_SUCCESS)
    {
        return 0ull;
    }

    AllocationInfo alloc = {
        allocation,
        memReq->size
    };

    uint64_t id = gNextId++;
    hmput(gAllocationTable, id, alloc);
    *pageCount = 1ull;

    return id;
}

VkResult KtxVmaBindBuffer(VkBuffer buffer, uint64_t allocId) {
    return vmaBindBufferMemory(vkinfo.allocator, hmget(gAllocationTable, allocId).allocation, buffer);
}

VkResult KtxVmaBindImage(VkImage image, uint64_t allocId) {
    return vmaBindImageMemory(vkinfo.allocator, hmget(gAllocationTable, allocId).allocation, image);
}

VkResult KtxVmaMapMemory(uint64_t allocId, uint64_t pageNumber, VkDeviceSize* mapLength, void** dataPtr) {
    AllocationInfo alloc = hmget(gAllocationTable, allocId);
    *mapLength = alloc.mapSize;
    return vmaMapMemory(vkinfo.allocator, alloc.allocation, dataPtr);
}

void KtxVmaUnmapMemory(uint64_t allocId, uint64_t pageNumber) {
    vmaUnmapMemory(vkinfo.allocator, hmget(gAllocationTable, allocId).allocation);
}

void KtxVmaFreeMemory(uint64_t allocId) {
    vmaFreeMemory(vkinfo.allocator, hmget(gAllocationTable, allocId).allocation);
    hmdel(gAllocationTable, allocId);
}

static ktxVulkanTexture_subAllocatorCallbacks callbacks = {
        .allocMemFuncPtr = KtxVmaAlloc,
        .bindBufferFuncPtr = KtxVmaBindBuffer,
        .bindImageFuncPtr = KtxVmaBindImage,
        .memoryMapFuncPtr = KtxVmaMapMemory,
        .memoryUnmapFuncPtr = KtxVmaUnmapMemory,
        .freeMemFuncPtr = KtxVmaFreeMemory
};

bool loadKtxEx(asset_info_t* uploadInfo, ktx_texture_t* outTexture, VkSamplerCreateInfo samplerInfo) {
    off64_t size;
    void* buffer = readAssetToBuffer(uploadInfo, &size);
    if(buffer == NULL) {
        return NULL;
    }

    ktxTexture* ktxTexture = NULL;
    if(ktxTexture_CreateFromMemory(
            (const ktx_uint8_t *) buffer, (ktx_size_t) size,
            KTX_TEXTURE_CREATE_NO_FLAGS, &ktxTexture) != KTX_SUCCESS) {
        free(buffer);
        LOGI("Failed to load ktxTexture \"%s\"", uploadInfo->path);
        return false;
    }
    if(ktxTexture_NeedsTranscoding(ktxTexture)) {
        // NOTE: assumptions were made here
        KTX_error_code transcodeResult = ktxTexture2_TranscodeBasis((ktxTexture2*)ktxTexture, KTX_TTF_ETC2_RGBA, 0);
        if(transcodeResult != KTX_SUCCESS) {
            LOGE("Failed to transcode ktxTexture \"%s\" due to error %i", uploadInfo->path, transcodeResult);
            free(buffer);
            return false;
        }
    }

    ktxVulkanDeviceInfo deviceInfo = {0};
    ktxVulkanDeviceInfo_Construct(
        &deviceInfo,
        vkinfo.physicalDevice,
        vkinfo.device,
        vkinfo.graphicsQueue,
        vkinfo.commandPool,
        NULL
    );

    ktxVulkanTexture kvx = {0};
    KTX_error_code uploadResult = ktxTexture_VkUploadEx_WithSuballocator(ktxTexture, &deviceInfo, &kvx,
                                           VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                           &callbacks);

    if (uploadResult != KTX_SUCCESS) {
        LOGE("Failed to upload KTX texture to GPU: %d", uploadResult);
        ktxTexture_Destroy(ktxTexture);
        ktxVulkanDeviceInfo_Destruct(&deviceInfo);
        free(buffer);
        return false;
    }

    outTexture->image       = kvx.image;
    outTexture->width       = ktxTexture->baseWidth;
    outTexture->height      = ktxTexture->baseHeight;
    outTexture->depth       = ktxTexture->baseDepth;
    outTexture->mipLevels   = ktxTexture->numLevels;
    outTexture->arrayLayers = kvx.layerCount;
    outTexture->format      = kvx.imageFormat;
    uint64_t allocId        = (uint64_t)kvx.deviceMemory;
    outTexture->allocation  = hmget(gAllocationTable, allocId).allocation;

    VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = outTexture->image,
            .viewType = kvx.viewType,
            .format = outTexture->format,
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = outTexture->mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = kvx.layerCount
            }
    };

    if (vkCreateImageView(vkinfo.device, &viewInfo, NULL, &outTexture->view) != VK_SUCCESS) {
        LOGE("Failed to create image view for KTX texture");
        ktxTexture_Destroy(ktxTexture);
        ktxVulkanDeviceInfo_Destruct(&deviceInfo);
        ktxVulkanTexture_Destruct_WithSuballocator(&kvx, vkinfo.device, NULL, &callbacks);
        free(buffer);
        return false;
    }

    samplerInfo.maxLod = (float)outTexture->mipLevels;

    if (vkCreateSampler(vkinfo.device, &samplerInfo, NULL, &outTexture->sampler) != VK_SUCCESS) {
        LOGE("Failed to create image view for KTX texture");
        vkDestroyImageView(vkinfo.device, outTexture->view, NULL);
        ktxTexture_Destroy(ktxTexture);
        ktxVulkanDeviceInfo_Destruct(&deviceInfo);
        ktxVulkanTexture_Destruct_WithSuballocator(&kvx, vkinfo.device, NULL, &callbacks);
        free(buffer);
        return false;
    }

    ktxTexture_Destroy(ktxTexture);
    ktxVulkanDeviceInfo_Destruct(&deviceInfo);
    free(buffer);

    LOGI("Created a KTX texture from %s successfully with image: %p, view %p, sampler %p", uploadInfo->path, outTexture->image, outTexture->view, outTexture->sampler);

    return true;
}

bool loadKtx(asset_info_t* uploadInfo, ktx_texture_t* outTexture) {
    return loadKtxEx(uploadInfo, outTexture, (VkSamplerCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .maxAnisotropy = 16.0f,
    });
}