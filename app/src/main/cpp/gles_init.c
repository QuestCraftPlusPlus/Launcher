//
// Created by maks on 03.12.2024.
//
#include "gles_init.h"


#define LOG_TAG __FILE_NAME__
#include "log.h"

egl_info_t egl_info;

bool initOpenGLES() {
    EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig eglConfig;
    EGLContext eglContext = EGL_NO_CONTEXT;
    EGLSurface eglSurface = EGL_NO_SURFACE;

    if(eglDisplay == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGLDisplay: %x", eglGetError());
        goto fail_init;
    }
    if(eglInitialize(eglDisplay, NULL, NULL) != EGL_TRUE) {
        LOGE("Failed to initialize EGL: %x", eglGetError());
        goto fail_init;
    }
    eglSwapInterval(eglDisplay, 1);
    const EGLint configAttributes[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_CONFORMANT, EGL_OPENGL_ES3_BIT,
            EGL_NONE};
    EGLint num_config = 0;
    if(eglChooseConfig(eglDisplay, configAttributes, &eglConfig, 1, &num_config) != EGL_TRUE) {
        LOGE("Failed to get an EGL configuration: %x", eglGetError());
        goto fail_term;
    }
    if(num_config == 0) {
        LOGE("%s", "Unsupported configuration");
        goto fail_term;
    }
    const EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
#ifdef DEBUG
        EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#endif
        EGL_NONE
    };
    eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttributes);
    if(eglContext == EGL_NO_CONTEXT) {
        LOGE("Failed to create OpenGL ES context: %x", eglGetError());
        goto fail_term;
    }
    const EGLint pbufferAttributes[] = {
            EGL_WIDTH, 16,
            EGL_HEIGHT, 16,
            EGL_NONE
    };
    eglSurface = eglCreatePbufferSurface(eglDisplay, eglConfig, pbufferAttributes);
    if(eglSurface == EGL_NO_SURFACE) {
        LOGE("Failed to create pbuffer surface: %x", eglGetError());
        goto fail_context;
    }
    egl_info.display = eglDisplay;
    egl_info.config = eglConfig;
    egl_info.context = eglContext;
    egl_info.surface = eglSurface;
    return true;
    fail_context:
    eglDestroyContext(eglDisplay, eglContext);
    fail_term:
    eglTerminate(eglDisplay);
    fail_init:
    return false;
}

bool makeContextCurrent() {
    return eglMakeCurrent(
            egl_info.display,
            egl_info.surface,
            egl_info.surface,
            egl_info.context
            ) == EGL_TRUE;
}

void makeContextNotCurrent() {
    eglMakeCurrent(egl_info.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void destroyOpenGLES() {
    LOGE("Destroying GLES Context!");
    eglDestroySurface(egl_info.display, egl_info.surface);
    eglDestroyContext(egl_info.display, egl_info.context);
    eglTerminate(egl_info.display);
}