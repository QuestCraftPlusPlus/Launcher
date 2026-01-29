//
// Created by maks on 05.12.2024.
//

#ifndef QCXR_LIGHTMAP_MODEL_GLES_PROGRAM_H
#define QCXR_LIGHTMAP_MODEL_GLES_PROGRAM_H

#include <GLES2/gl2.h>
#include <stdbool.h>

typedef struct {
    GLuint name;
    struct {
        GLuint matrixBlockBinding;
        GLuint textureSampler;
        GLuint lightingSampler;
        GLuint projectionIndex;
    } u;
    struct {
        GLuint position;
        GLuint texAndLightCoord;
    } v;
} world_model_render_program_t;

bool lm_model_render_program_create(world_model_render_program_t* program);

#endif //QCXR_LIGHTMAP_MODEL_GLES_PROGRAM_H
