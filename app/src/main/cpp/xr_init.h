//
// Created by maks on 03.12.2024.
//

#ifndef QCXR_XR_INIT_H
#define QCXR_XR_INIT_H
#include "xr_include.h"
#include <stdbool.h>
#include <jni.h>

typedef struct {
    uint32_t width, height;
    XrSwapchain swapchain;
    uint32_t swapchainImageCount;
    VkImage* swapchainImages;
} render_target_t;

typedef struct {
    XrInstance instance;
    XrSystemId systemId;
    XrSession session;
    XrSpace localReferenceSpace;
    XrViewConfigurationType configurationType;
    render_target_t renderTarget;
    uint32_t nViews;
    int dominantHand;
    bool hasSession;
} xr_state_t;

typedef struct {
    JavaVM* applicationVm;
    jobject applicationActivity;
} android_jni_data_t;

extern xr_state_t xrinfo;

bool xriInitialize(android_jni_data_t* jniData);
bool xriInitSession();
bool xriStartSession();
void xriEndSession();
void xriFreeSession();
void xriFree();

#endif //QCXR_XR_INIT_H
