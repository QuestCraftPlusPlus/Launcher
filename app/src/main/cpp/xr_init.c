//
// Created by maks on 03.12.2024.
//
#include "xr_include.h"
#include "xr_init.h"
#include "gles_init.h"

#include <string.h>
#include <stdlib.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

xr_state_t xrinfo;


static void loaderInitialize(android_jni_data_t* jniData) {
    PFN_xrInitializeLoaderKHR initializeLoader;
    if(XR_SUCCEEDED(xrGetInstanceProcAddr(
            XR_NULL_HANDLE,
            "xrInitializeLoaderKHR",
            (PFN_xrVoidFunction*)(&initializeLoader)))
            ) {
        XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfoAndroid.applicationContext = jniData->applicationActivity;
        loaderInitInfoAndroid.applicationVM = jniData->applicationVm;
        initializeLoader((const XrLoaderInitInfoBaseHeaderKHR*)&loaderInitInfoAndroid);
    }
}

typedef XrResult (*pXrEnumerateDisplayRefreshRatesFB)(
        XrSession                                   session,
        uint32_t                                    displayRefreshRateCapacityInput,
        uint32_t*                                   displayRefreshRateCountOutput,
        float*                                      displayRefreshRates);

typedef XrResult (*pXrRequestDisplayRefreshRateFB)(
        XrSession                                   session,
        float                                       displayRefreshRate);

static bool createXrInstance(android_jni_data_t* jniData) {

    static const char* instanceExtensions[] = {
            XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
            XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
            XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME
    };

    XrInstanceCreateInfoAndroidKHR androidCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidCreateInfo.applicationActivity = jniData->applicationActivity;
    androidCreateInfo.applicationVM = jniData->applicationVm;

    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = &androidCreateInfo;
    createInfo.enabledExtensionNames = instanceExtensions;
    createInfo.enabledExtensionCount = sizeof(instanceExtensions) / sizeof(instanceExtensions[0]);

    strcpy(createInfo.applicationInfo.applicationName, "HelloXR");
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

    XR_FAILRETURN(xrCreateInstance(&createInfo, &xrinfo.instance), false);

    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XR_FAILGOTO(xrGetSystem(xrinfo.instance, &systemInfo, &xrinfo.systemId), destroy_instance);

    return true;
    destroy_instance:
    xrDestroyInstance(xrinfo.instance);
    return false;
}

static bool getXrGraphicsRequirements(XrGraphicsRequirementsOpenGLESKHR* graphicsRequirements) {
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGlesGraphicsRequirements;
    XR_FAILRETURN(
            xrGetInstanceProcAddr(xrinfo.instance,"xrGetOpenGLESGraphicsRequirementsKHR",(PFN_xrVoidFunction*)&xrGetOpenGlesGraphicsRequirements),
            false);
    XR_FAILRETURN(xrGetOpenGlesGraphicsRequirements(xrinfo.instance, xrinfo.systemId, graphicsRequirements), false);
    return true;
}

static bool initializeGLESSession() {
    XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};

    if(!getXrGraphicsRequirements(&graphicsRequirements)) {
        LOGE("%s", "Failed to detect OpenXR runtime version requirements");
        return false;
    }

    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    const XrVersion currentApiVersion = XR_MAKE_VERSION(major, minor, 0);

    if(graphicsRequirements.minApiVersionSupported > currentApiVersion) {
        LOGE("OpenGL ES version %i.%i not supported by OpenXR runtime", major, minor);
        return false;
    }

    XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingOpenGLES = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBindingOpenGLES.display = egl_info.display;
    graphicsBindingOpenGLES.context = egl_info.context;
    graphicsBindingOpenGLES.config = egl_info.config;

    XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &graphicsBindingOpenGLES;
    sessionCreateInfo.systemId = xrinfo.systemId;

    XR_FAILRETURN(xrCreateSession(xrinfo.instance, &sessionCreateInfo, &xrinfo.session), false);
    return true;
}

static bool setRefreshRate() {
    pXrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB;
    XR_FAILRETURN(xrGetInstanceProcAddr(xrinfo.instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)&xrEnumerateDisplayRefreshRatesFB), false);

    uint32_t displayRefreshRateCount;
    XR_FAILRETURN(xrEnumerateDisplayRefreshRatesFB(xrinfo.session, 0, &displayRefreshRateCount, NULL), false);

    float displayRefreshRates[displayRefreshRateCount];
    XR_FAILRETURN(xrEnumerateDisplayRefreshRatesFB(xrinfo.session, displayRefreshRateCount, &displayRefreshRateCount, displayRefreshRates), false);

    pXrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB;
    XR_FAILRETURN(xrGetInstanceProcAddr(xrinfo.instance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)&xrRequestDisplayRefreshRateFB), false);

    for (uint32_t i = 0; i < displayRefreshRateCount; i++) {
        LOGI("Display refresh rate: %f", displayRefreshRates[i]);
    }

    XR_FAILRETURN(xrRequestDisplayRefreshRateFB(xrinfo.session, displayRefreshRates[displayRefreshRateCount -1]), false);
    return true;
}

static bool createReferenceSpace() {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    referenceSpaceCreateInfo.poseInReferenceSpace.orientation.w = 1;
    XR_FAILRETURN(xrCreateReferenceSpace(xrinfo.session, &referenceSpaceCreateInfo, &xrinfo.localReferenceSpace), false);
    return true;
}

static bool createViewSurface() {
    xrinfo.configurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    uint32_t viewCount;

    XR_FAILRETURN(xrEnumerateViewConfigurationViews(xrinfo.instance, xrinfo.systemId, xrinfo.configurationType, 0, &viewCount, NULL), false);

    XrViewConfigurationView configurationViews[viewCount];
    for(uint32_t i = 0; i < viewCount; i++) {
        XrViewConfigurationView defaultConfigView = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
        memcpy(&configurationViews[i], &defaultConfigView, sizeof(XrViewConfigurationView));
    }

    XR_FAILRETURN(xrEnumerateViewConfigurationViews(xrinfo.instance, xrinfo.systemId, xrinfo.configurationType, viewCount, &viewCount, configurationViews), false);

    const XrViewConfigurationView *viewConfig = &configurationViews[0];
    uint32_t width = viewConfig->recommendedImageRectWidth;
    uint32_t height = viewConfig->recommendedImageRectHeight;
    XrSwapchain swapchain;
    XrSwapchainCreateInfo swapchainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainCreateInfo.usageFlags =
            XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.format = GL_SRGB8_ALPHA8;
    swapchainCreateInfo.width = width;
    swapchainCreateInfo.height = height;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.arraySize = 2;
    swapchainCreateInfo.mipCount = 1;
    swapchainCreateInfo.sampleCount = 1;

    XR_FAILRETURN(xrCreateSwapchain(xrinfo.session, &swapchainCreateInfo, &swapchain), false);

    uint32_t imageCount = 0;
    XrResult xrEnumerateSwapchainImages_result = xrEnumerateSwapchainImages(swapchain, 0, &imageCount, NULL);
    // Even if xrEnumerateSwapchainImages fails, this will still initialize with a size of 0
    // It wouldn't matter though since if the call fails we go straight to resource cleanup
    XrSwapchainImageOpenGLESKHR glesImages[imageCount];

    XR_FAILRETURN(xrEnumerateSwapchainImages_result, false);

    for (uint32_t j = 0; j < imageCount; j++) {
        XrSwapchainImageOpenGLESKHR defaultImage = {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR};
        memcpy(&glesImages[j], &defaultImage, sizeof(XrSwapchainImageOpenGLESKHR));
    }

    XR_FAILGOTO(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount,
                                           (XrSwapchainImageBaseHeader *) &glesImages), free_swapchain);

    xrinfo.renderTarget.swapchainTextures = calloc(imageCount, sizeof(GLuint));

    xrinfo.renderTarget.swapchain = swapchain;
    xrinfo.renderTarget.width = width;
    xrinfo.renderTarget.height = height;
    xrinfo.renderTarget.swapchainTextureCount = imageCount;
    for (uint32_t j = 0; j < imageCount; j++) {
        xrinfo.renderTarget.swapchainTextures[j] = glesImages[j].image;
    }
    xrinfo.nViews = viewCount;
    xrinfo.dominantHand = 1; // TODO: put this somewhere else, load user preferences and see there.

    return true;
    free_swapchain:
    xrDestroySwapchain(xrinfo.renderTarget.swapchain);
    return false;
}

bool xriStartSession() {
    XrSessionBeginInfo beginInfo = {XR_TYPE_SESSION_BEGIN_INFO};
    beginInfo.primaryViewConfigurationType = xrinfo.configurationType;
    XR_FAILRETURN(xrBeginSession(xrinfo.session, &beginInfo), false);
    xrinfo.hasSession = true;
    return true;
}

void xriEndSession() {
    xrEndSession(xrinfo.session);
}

bool xriInitSession() {
    if(!initializeGLESSession()) return false;
    if(!createReferenceSpace()) goto fail;
    if(!createViewSurface()) goto fail;
    for(int j = 0; j < xrinfo.renderTarget.swapchainTextureCount; j++) {
        LOGI("Swapchain texture: %i", xrinfo.renderTarget.swapchainTextures[j]);
    }
    setRefreshRate();
    xrinfo.hasSession = true;
    return true;
    fail:
    xrDestroySession(xrinfo.session);
    return false;
}

void xriFreeSession() {
    xrDestroySession(xrinfo.session);
    void* swArray = xrinfo.renderTarget.swapchainTextures;
    xrinfo.renderTarget.swapchainTextures = NULL;
    free(swArray);
    xrinfo.hasSession = false;
}

bool xriInitialize(android_jni_data_t* jniData) {
    loaderInitialize(jniData);
    return createXrInstance(jniData);
}

void xriFree() {
    if(xrinfo.hasSession) xriFreeSession();
    xrDestroyInstance(xrinfo.instance);
}