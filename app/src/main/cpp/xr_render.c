//
// Created by maks on 03.12.2024.
//

#include "xr_render.h"
#include "xr_init.h"
#include "xr_include.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"
#include "xr_input.h"

static const XrCompositionLayerProjectionView defaultCompositionLayerProjectionView  = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
static const XrView defaultView = {XR_TYPE_VIEW};
//static

void initializeBeginEndState(frame_begin_end_state_t* state) {
    XrCompositionLayerProjection compositionLayerProjection  = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};

    state->projectionViews = calloc(xrinfo.nViews, sizeof(XrCompositionLayerProjectionView));

    for(uint32_t i = 0; i < xrinfo.nViews; i++) {
        memcpy(&state->projectionViews[i], &defaultCompositionLayerProjectionView, sizeof(XrCompositionLayerProjectionView));
    }

    state->layerProjection[0] = calloc(1, sizeof(XrCompositionLayerProjection));
    memcpy(state->layerProjection[0], &compositionLayerProjection, sizeof(XrCompositionLayerProjection));

    state->layerProjection[0]->space = xrinfo.localReferenceSpace;
    state->layerProjection[0]->viewCount = xrinfo.nViews;
    state->layerProjection[0]->views = state->projectionViews;

    XrFrameEndInfo frameEndInfo = {XR_TYPE_FRAME_END_INFO};
    memcpy(&state->frameEndInfo, &frameEndInfo, sizeof(XrFrameEndInfo));
    state->frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    state->frameEndInfo.layerCount = 1;
    state->frameEndInfo.layers = (const XrCompositionLayerBaseHeader *const *) state->layerProjection;
}

void freeBeginEndState(frame_begin_end_state_t* state) {
    free(state->projectionViews);
    free(state->layerProjection[0]);
}

bool beginFrame(frame_begin_end_state_t* state) {
    XrFrameWaitInfo frameWaitInfo  = {XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState = {XR_TYPE_FRAME_STATE};

    XR_FAILRETURN(xrWaitFrame(xrinfo.session, &frameWaitInfo, &frameState), false);

    state->displayTime = frameState.predictedDisplayTime;
    pollActions(state->displayTime);

    XrFrameBeginInfo frameBegin = {XR_TYPE_FRAME_BEGIN_INFO};

    XR_FAILRETURN(xrBeginFrame(xrinfo.session, &frameBegin), false);

    XrViewState viewState = {XR_TYPE_VIEW_STATE};
    XrViewLocateInfo locateInfo = {XR_TYPE_VIEW_LOCATE_INFO};
    locateInfo.viewConfigurationType = xrinfo.configurationType;
    locateInfo.displayTime = frameState.predictedDisplayTime;
    locateInfo.space = xrinfo.localReferenceSpace;

    uint32_t nViews = xrinfo.nViews;
    uint32_t nLocatedViews;
    XrView views[nViews];

    for(uint32_t i = 0; i < nViews; i++) {
        memcpy(&views[i], &defaultView, sizeof(XrView));
    }

    XR_FAILRETURN(xrLocateViews(xrinfo.session, &locateInfo, &viewState, nViews, &nLocatedViews, views), false);

    assert(nLocatedViews == nViews);

    XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    XrSwapchainImageWaitInfo swapchainWaitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    swapchainWaitInfo.timeout = XR_INFINITE_DURATION;

    XrSwapchain swapchain = xrinfo.renderTarget.swapchain;

    XR_FAILRETURN(xrAcquireSwapchainImage(swapchain, &acquireInfo, &state->frame.imageIndex), false);
    XR_FAILRETURN(xrWaitSwapchainImage(swapchain, &swapchainWaitInfo), false);

    XrRect2Di imageRect;
    imageRect.offset.x = 0;
    imageRect.offset.y = 0;
    imageRect.extent.width = (int32_t)xrinfo.renderTarget.width;
    imageRect.extent.height = (int32_t)xrinfo.renderTarget.height;

    state->frame.outputRect = imageRect;

    for(uint32_t i = 0; i < nViews; i++) {
        XrSwapchainSubImage subImage;
        subImage.swapchain = swapchain;
        subImage.imageRect = imageRect;
        subImage.imageArrayIndex = i;
        state->projectionViews[i].pose = views[i].pose;
        state->projectionViews[i].fov = views[i].fov;
        state->projectionViews[i].subImage = subImage;
    }
    return true;
}

void endFrame(frame_begin_end_state_t* state) {
    XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(xrinfo.renderTarget.swapchain, &releaseInfo);
    state->frameEndInfo.displayTime = state->displayTime;
    xrEndFrame(xrinfo.session, &state->frameEndInfo);
}
