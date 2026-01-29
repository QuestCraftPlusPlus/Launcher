//
// Created by maks on 03.12.2024.
//
#include <jni.h>
#include <EGL/egl.h>
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifndef QCXR_XR_EXTRA_MACROS
#define QCXR_XR_EXTRA_MACROS

#define XR_FAIL(x, s) do { \
    XrResult result = x; \
    if(result != XR_SUCCESS) {   \
        LOGE(#x" failed: %i", result); \
        s; \
    }\
} while(0)

#define XR_FAILGOTO(x, lb) XR_FAIL(x, goto lb)
#define XR_FAILRETURN(x, rv) XR_FAIL(x, return rv)

#endif