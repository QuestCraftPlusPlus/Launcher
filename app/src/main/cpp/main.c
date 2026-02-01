
#include "xr_init.h"
#include "xr_render.h"
#include "vk_init.h"
#include "renderer.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

#include "asset_buffer_read.h"
#include "xr_input.h"
#include <android/asset_manager_jni.h>

//
// Created by maks on 03.12.2024.
//

static AAssetManager *g_assetManager;
static android_jni_data_t jniData;
static jclass mainActivityClass;
static jclass xrActivityInputClass;
static jmethodID MainActivity_createSurfaceTexture;
static jmethodID MainActivity_updateSurfaceTexture;
static jmethodID MainActivity_performSystemExit;
static jmethodID XRActivityInput_clickScreenAtPosition;
static bool shouldStopJni;
JavaVM *globalVm;

static void performSystemExit(JNIEnv *env) {
    (*env)->CallStaticVoidMethod(env, mainActivityClass, MainActivity_performSystemExit);
}

static void updateSurfaceTexture(JNIEnv *env) {
    (*env)->CallStaticVoidMethod(env, mainActivityClass, MainActivity_updateSurfaceTexture);
}

static bool createSurfaceTexture(JNIEnv *env, int textureId) {
    return (*env)->CallStaticBooleanMethod(env, mainActivityClass, MainActivity_createSurfaceTexture, textureId);
}

void clickScreenAtPosition(JNIEnv *env, int x, int y) {
    (*env)->CallStaticVoidMethod(env, xrActivityInputClass, XRActivityInput_clickScreenAtPosition, x, y);
}

struct event_state {
    bool shouldRender;
    bool shouldShutdown;
};

static void process_session_state_changed(struct event_state *state, XrEventDataSessionStateChanged* eventData) {
    if(eventData->session != xrinfo.session) return;
    switch (eventData->state) {
        case XR_SESSION_STATE_READY:
            if(xriStartSession()) {
                state->shouldRender = true;
            }else {
                LOGE("Failed to start session, shutting down");
                state->shouldShutdown = true;
            }
            break;
        case XR_SESSION_STATE_STOPPING:
            LOGI("Stopping session and rendering...");
            xriEndSession();
            state->shouldRender = false;
            break;
        case XR_SESSION_STATE_EXITING:
        case XR_SESSION_STATE_LOSS_PENDING:
            state->shouldShutdown = true;
            break;
        default:
            break;
    }
}

static void process_single_event(struct event_state *state, XrEventDataBuffer* eventData) {
    switch (eventData->type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            state->shouldShutdown = true;
            break;
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            process_session_state_changed(state, (XrEventDataSessionStateChanged*) eventData);
            break;
        default:
            break;
    }
}

static bool poll_events(struct event_state *state) {
    const XrEventDataBuffer default_buf = {XR_TYPE_EVENT_DATA_BUFFER};
    XrEventDataBuffer eventData;
    XrResult pollResult;
    do {
        memcpy(&eventData, &default_buf, sizeof(XrEventDataBuffer));
        pollResult = xrPollEvent(xrinfo.instance, &eventData);
        process_single_event(state, &eventData);
    } while (pollResult == XR_SUCCESS && !state->shouldShutdown);
    return pollResult == XR_EVENT_UNAVAILABLE && !state->shouldShutdown;
}

static void* main_loop(void* data) {
    (void)data;
    JNIEnv *env;
    (*jniData.applicationVm)->AttachCurrentThread(jniData.applicationVm, &env, NULL);
    globalVm = jniData.applicationVm;
    if (!xriInitialize(&jniData)) goto fatal_exit;
    if (!initVulkan(xrinfo.instance, xrinfo.systemId)) goto free_xri;
    createActionSet();
    createDefaultActions();
    createSuggestedBindings();
    if(!xriInitSession()) goto free_vulkan;
    if (!initRenderer(g_assetManager)) goto free_vulkan;
    createActionPoses();
    attachActionSet();

    frame_begin_end_state_t state;

    initializeBeginEndState(&state);

    struct event_state event_state;

    while(true) {
        if(!poll_events(&event_state) || shouldStopJni) {
            goto exit;
        }
        if(!event_state.shouldRender) {
            usleep(8000);
            continue;
        }
        if(!beginFrame(&state)) goto exit;
//        updateSurfaceTexture(env);
        renderFrame(&state);
        endFrame(&state);
    }

    exit:
    freeBeginEndState(&state);
    cleanupRenderer();
    free_vulkan:
    destroyVulkan();
    free_xri:
    xriFree();
    fatal_exit:

    performSystemExit(env);

    pthread_exit(NULL);
}

JNIEXPORT void JNICALL
Java_com_qcxr_questcraft_MainActivity_start(JNIEnv *env, jobject mainActivity, jobject xrActivityInput, jobject assetManager) {
    (*env)->GetJavaVM(env, &jniData.applicationVm);
    jniData.applicationActivity = (*env)->NewGlobalRef(env, mainActivity);

    mainActivityClass = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, mainActivity));
    xrActivityInputClass = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, xrActivityInput));

    MainActivity_createSurfaceTexture = (*env)->GetStaticMethodID(env, mainActivityClass, "createSurfaceTexture", "(I)Z");
    MainActivity_updateSurfaceTexture = (*env)->GetStaticMethodID(env, mainActivityClass, "updateSurfaceTexture", "()V");
    MainActivity_performSystemExit = (*env)->GetStaticMethodID(env, mainActivityClass, "performSystemExit", "()V");
    XRActivityInput_clickScreenAtPosition = (*env)->GetStaticMethodID(env, xrActivityInputClass, "clickScreenAtPosition","(II)V");

    g_assetManager = AAssetManager_fromJava(env, assetManager);

    pthread_t thread;
    pthread_create(&thread, NULL, main_loop, NULL);
}

JNIEXPORT void JNICALL
Java_com_qcxr_questcraft_MainActivity_stop( JNIEnv *env, jobject thiz) {
    (void)env;
    (void)thiz;
    shouldStopJni = true;
}