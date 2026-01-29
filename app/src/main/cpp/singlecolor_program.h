//
// Created by CADIndie on 12/16/2024.
//

#ifndef QCXR_SINGLECOLOR_PROGRAM_H
#define QCXR_SINGLECOLOR_PROGRAM_H

#include <GLES2/gl2.h>
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
        GLuint color;
    } v;
} singlecolor_render_program_t;

bool singlecolor_program_create(singlecolor_render_program_t* program);

#endif //QCXR_SINGLECOLOR_PROGRAM_H
