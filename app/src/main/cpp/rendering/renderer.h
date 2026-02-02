//
// Created by maks on 12.12.2024.
//

#ifndef QCXR_RENDERER_H
#define QCXR_RENDERER_H
#include <stdbool.h>
#include <android/asset_manager.h>
#include "../xr/xr_render.h"
#include "renderer_types.h"

void renderFrame(frame_begin_end_state_t *state);
bool initRenderer(AAssetManager *assetManager);
void cleanupRenderer();

extern render_state_t vk_rs;

#endif //QCXR_RENDERER_H
