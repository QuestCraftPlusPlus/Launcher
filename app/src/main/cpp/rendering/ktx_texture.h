//
// Created by maks on 05.12.2024.
//

#ifndef QCXR_KTX_TEXTURE_H
#define QCXR_KTX_TEXTURE_H

#include "asset_buffer_read.h"
#include "vk_init.h"
#include "renderer_types.h"

#include <android/asset_manager.h>
#include <stdbool.h>

bool loadKtx(asset_info_t *uploadInfo, vk_texture_t *outTexture);

#endif //QCXR_KTX_TEXTURE_H
