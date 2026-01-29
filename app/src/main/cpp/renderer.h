//
// Created by maks on 12.12.2024.
//

#ifndef QCXR_RENDERER_H
#define QCXR_RENDERER_H
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <android/asset_manager.h>
#include "xr_render.h"

GLuint getRenderTargetName();
bool initRenderer(AAssetManager *assetManager);
void renderFrame(frame_begin_end_state_t *state);

#endif //QCXR_RENDERER_H
