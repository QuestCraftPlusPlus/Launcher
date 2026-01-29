//
// Created by maks on 10.12.2024.
//
#include "asset_buffer_read.h"
#include <stdlib.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

void* readAssetToBuffer(asset_info_t* uploadInfo, off64_t* assetSize) {
    AAsset *textureAsset = AAssetManager_open(uploadInfo->assetManager, uploadInfo->path, AASSET_MODE_STREAMING);
    if(textureAsset == NULL) {
        LOGE("Failed to open asset \"%s\"", uploadInfo->path);
        return NULL;
    }

    off64_t length = AAsset_getLength64(textureAsset);
    if(length == -1) {
        LOGE("Failed to get length of \"%s\"", uploadInfo->path);
        goto close_asset;
    }

    void* buffer = malloc(length);
    if(buffer == NULL) {
        LOGE("Failed to allocate asset buffer");
        goto close_asset;
    }

    if(AAsset_read(textureAsset, buffer, (size_t)length) < length) {
        LOGE("Failed to fully read asset \"%s\"", uploadInfo->path);
        goto free_buffer;
    }

    AAsset_close(textureAsset);
    *assetSize = length;
    return buffer;

    free_buffer:
    free(buffer);
    close_asset:
    AAsset_close(textureAsset);
    return NULL;
}