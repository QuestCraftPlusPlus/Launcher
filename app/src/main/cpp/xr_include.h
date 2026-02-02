//
// Created by maks on 03.12.2024.
//
#include <jni.h>
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vk_mem_alloc.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifndef QCXR_XR_EXTRA_MACROS
#define QCXR_XR_EXTRA_MACROS

#define XR_FAIL(x, s) do { \
    XrResult result = x; \
    if(result != XR_SUCCESS) { \
        LOGE(#x" failed: %i", result); \
        s; \
    }\
} while(0)

#define XR_FAILGOTO(x, lb) XR_FAIL(x, goto lb)
#define XR_FAILRETURN(x, rv) XR_FAIL(x, return rv)

#define VK_FAIL(x, s) do { \
    VkResult result = x; \
    if (result != VK_SUCCESS) { \
        LOGE(#x" failed: %i", result); \
        s; \
    } \
} while (0)

#define VK_FAILGOTO(x, lb) VK_FAIL(x, goto lb)
#define VK_FAILRETURN(x, rv) VK_FAIL(x, return rv)

#endif