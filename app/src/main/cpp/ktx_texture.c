//
// Created by maks on 05.12.2024.
//

#include "ktx_texture.h"
#include "gl_safepoint.h"
#include <ktx.h>
#include <stdlib.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

bool loadKtx(asset_info_t* uploadInfo, GLuint* texture, GLenum* target) {
    off64_t size;
    void* buffer = readAssetToBuffer(uploadInfo, &size);
    if(buffer == NULL) {
        return NULL;
    }

    GL_SAFEPOINT;
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
    glGenTextures(1, texture);
    GLenum glError = glGetError();
    ktx_error_code_e uploadError = ktxTexture_GLUpload(ktxTexture, texture, target, &glError);
    ktxTexture_Destroy(ktxTexture);
    free(buffer);
    if(uploadError == KTX_SUCCESS) return true;
    glDeleteTextures(1, texture);
    switch(uploadError) {
        case KTX_GL_ERROR:
            LOGE("Failed to upload ktxTexture \"%s\" into OpenGL (error %x)", uploadInfo->path, uploadError);
            return false;
        case KTX_UNSUPPORTED_TEXTURE_TYPE:
            LOGE("KTX library does not support texture type of ktxTexture \"%s\"", uploadInfo->path);
            return false;
        case KTX_INVALID_VALUE:
            LOGE("KTX \"%s\" invalid value", uploadInfo->path);
            return false;
        default:
            LOGE("ktxTexture \"%s\" upload error %i", uploadInfo->path, uploadError);
            return false;
    }
}