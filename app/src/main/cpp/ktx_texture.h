//
// Created by maks on 05.12.2024.
//

#ifndef QCXR_KTX_TEXTURE_H
#define QCXR_KTX_TEXTURE_H

#include "asset_buffer_read.h"

#include <android/asset_manager.h>
#include <GLES2/gl2.h>
#include <stdbool.h>


bool loadKtx(asset_info_t* uploadInfo, GLuint* texture, GLenum* target);

#endif //QCXR_KTX_TEXTURE_H
