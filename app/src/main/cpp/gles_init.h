//
// Created by maks on 03.12.2024.
//

#ifndef QCXR_GLES_INIT_H
#define QCXR_GLES_INIT_H

#include <stdbool.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

typedef struct {
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    bool has_multiview;
} egl_info_t;

extern egl_info_t egl_info;

extern bool initOpenGLES();
extern void destroyOpenGLES();
extern bool makeContextCurrent();
extern void makeContextNotCurrent();

#endif //QCXR_GLES_INIT_H
