//
// Created by maks on 12.12.2024.
//

#ifndef QCXR_RENDERTARGET_BLIT_PROGRAM_H
#define QCXR_RENDERTARGET_BLIT_PROGRAM_H

#include <stdbool.h>

typedef struct {
    GLuint name;
    struct {
        GLuint matrixBlockBinding;
        GLuint rtSampler;
        GLuint projectionIndex;
    } u;
    struct {
        GLuint position;
        GLuint texCoord;
    } v;
} rendertarget_blit_render_program_t;

bool rendertarget_blit_program_create(rendertarget_blit_render_program_t* program);

#endif //QCXR_RENDERTARGET_BLIT_PROGRAM_H
