//
// Created by maks on 03.12.2024.
//

#include <stdbool.h>
#include "xr_include.h"

#ifndef QCXR_XR_RENDER_H
#define QCXR_XR_RENDER_H

typedef struct {
    XrRect2Di outputRect;
    uint32_t imageIndex;
} frame_info_t;

typedef struct {
    XrTime displayTime;
    frame_info_t frame;
    XrCompositionLayerProjectionView* projectionViews;
    XrCompositionLayerProjection* layerProjection[1];
    XrFrameEndInfo frameEndInfo;
} frame_begin_end_state_t;

void initializeBeginEndState(frame_begin_end_state_t* state);
bool beginFrame(frame_begin_end_state_t* state);
void endFrame(frame_begin_end_state_t* state);
void freeBeginEndState(frame_begin_end_state_t* state);

#endif //QCXR_XR_RENDER_H

