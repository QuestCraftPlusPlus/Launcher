//
// Created by maks on 12.12.2024.
//

#include "renderer.h"
#include "lightmap_model_gles_program.h"
#include "rendertarget_blit_program.h"
#include "singlecolor_program.h"
#include "asset_buffer_read.h"
#include "ktx_texture.h"
#include "gl_safepoint.h"
#include "xr_init.h"
#include "multiview_detect.h"
#include "gles_init.h"
#include "xr_linear_algebra.h"
#include "xr_input.h"

#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

typedef struct {
    GLuint vbo;
    GLuint vao;
    GLuint program;
    GLenum drawMode;
    GLuint elementCount;
} model_t;

typedef struct {
    GLuint name;
    GLenum target;
} texture_t;

struct {
    texture_t atlas, light, surface;
    model_t worldModel, targetRectModel, leftControllerRay, rightControllerRay;
    world_model_render_program_t worldProgram;
    rendertarget_blit_render_program_t blitProgram;
    singlecolor_render_program_t singlecolorProgram;
    XrExtent2Di depthSize;
    GLuint framebuffer, depthOutput, matrixBuffer;
} rs;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantParameter"
static bool loadTexture(GLenum target, texture_t* texture, asset_info_t * uploadInfo) {
    texture->target = target;
    return loadKtx(uploadInfo, &texture->name, &texture->target);
}
#pragma clang diagnostic pop

static bool createSurfaceTextureId() {
    GL_SAFEPOINT;
    glGenTextures(1, &rs.surface.name);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, rs.surface.name);
    rs.surface.target = GL_TEXTURE_EXTERNAL_OES;
    GL_RETURN(true, false, "Failed to create external texture for render target: %x", error);
}

static bool loadTextures(AAssetManager* assetManager) {
    asset_info_t atexUploadInfo = {assetManager, "atlas_texture.ktx"};
    asset_info_t ltexUploadInfo = {assetManager, "light_texture.ktx"};
    return loadTexture(GL_TEXTURE_2D, &rs.atlas, &atexUploadInfo) &&
           loadTexture(GL_TEXTURE_2D, &rs.light, &ltexUploadInfo);
}

static bool initWorldModelDrawLayout(model_t *model, size_t modeLength, const world_model_render_program_t program) {
    GL_SAFEPOINT;
    glGenVertexArrays(1, &model->vao);
    glBindVertexArray(model->vao);

    glBindBuffer(GL_ARRAY_BUFFER, model->vbo);

    glEnableVertexAttribArray(program.v.position);
    glEnableVertexAttribArray(program.v.texAndLightCoord);

    GLsizei vertexSize = 8 * sizeof(GLfloat);
    const void* positionOffset = (const void*)(0 * sizeof(GLfloat));
    const void* texLightOffset = (const void*)(3 * sizeof(GLfloat));

    glVertexAttribPointer(program.v.position, 3, GL_FLOAT, GL_FALSE, vertexSize, positionOffset);
    glVertexAttribPointer(program.v.texAndLightCoord, 4, GL_FLOAT, GL_FALSE, vertexSize, texLightOffset);

    model->program = program.name;
    model->elementCount = modeLength / vertexSize;
    LOGI("MDDL element count: %i size %lu", model->elementCount, modeLength);

    GL_RETURN(true, false, "initWorldModelDrawLayout failed: %x", error);
}

static bool initTvModelDrawLayout(model_t *model, size_t modeLength, const rendertarget_blit_render_program_t program) {
    GL_SAFEPOINT;
    glGenVertexArrays(1, &model->vao);
    glBindVertexArray(model->vao);

    glBindBuffer(GL_ARRAY_BUFFER, model->vbo);

    glEnableVertexAttribArray(program.v.position);
    glEnableVertexAttribArray(program.v.texCoord);

    GLsizei vertexSize = 5 * sizeof(GLfloat);
    const void* positionOffset = (const void*)(0 * sizeof(GLfloat));
    const void* texOffset = (const void*)(3 * sizeof(GLfloat));

    glVertexAttribPointer(program.v.position, 3, GL_FLOAT, GL_FALSE, vertexSize, positionOffset);
    glVertexAttribPointer(program.v.texCoord, 2, GL_FLOAT, GL_FALSE, vertexSize, texOffset);

    model->program = program.name;
    model->elementCount = modeLength / vertexSize;
    LOGI("MDDL element count: %i size %lu", model->elementCount, modeLength);

    GL_RETURN(true, false, "initTvModelDrawLayout failed: %x", error);
}

static bool initLineModelDrawLayout(model_t *model, size_t modeLength, const singlecolor_render_program_t program) {
    GL_SAFEPOINT;
    glGenVertexArrays(1, &model->vao);
    glBindVertexArray(model->vao);

    glBindBuffer(GL_ARRAY_BUFFER, model->vbo);

    glEnableVertexAttribArray(program.v.position);
    glEnableVertexAttribArray(program.v.color);

    GLsizei vertexSize = 6 * sizeof(GLfloat);
    const void* positionOffset = (const void*)(0 * sizeof(GLfloat));
    const void* colorOffset = (const void*)(3 * sizeof(GLfloat));

    glVertexAttribPointer(program.v.position, 3, GL_FLOAT, GL_FALSE, vertexSize, positionOffset);
    glVertexAttribPointer(program.v.color, 3, GL_FLOAT, GL_FALSE, vertexSize, colorOffset);

    model->program = program.name;
    model->elementCount = modeLength / vertexSize;
    LOGI("MDDL element count: %i size %lu", model->elementCount, modeLength);

    GL_RETURN(true, false, "initLineModelDrawLayout failed: %x", error);
}

static bool createLineModel(model_t *model) {
    size_t modelLength = sizeof(GLfloat) * 12;
    GL_SAFEPOINT;
    glGenBuffers(1, &model->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) modelLength, NULL, GL_DYNAMIC_DRAW);
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        LOGE("Failed to create line model VBO: %x", error);
    }
    if(!initLineModelDrawLayout(model, modelLength, rs.singlecolorProgram)) return false;
    model->drawMode = GL_LINES;
    return true;
}

static void uploadLineModelData(model_t *model, XrVector3f color, XrVector3f startPos, XrVector3f endPos) {
    GLfloat dataFloats[12];
    dataFloats[0] = startPos.x;
    dataFloats[1] = startPos.y;
    dataFloats[2] = startPos.z;
    dataFloats[3] = color.x;
    dataFloats[4] = color.y;
    dataFloats[5] = color.z;
    dataFloats[6] = endPos.x;
    dataFloats[7] = endPos.y;
    dataFloats[8] = endPos.z;
    dataFloats[9] = color.x;
    dataFloats[10] = color.y;
    dataFloats[11] = color.z;
    glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(dataFloats), dataFloats);
}

static bool loadModelDrawData(model_t* model, asset_info_t* assetInfo, off64_t *size) {
    off64_t length;
    void* buffer = readAssetToBuffer(assetInfo, &length);
    if(buffer == NULL) {
        LOGE("Failed to allocate model asset buffer");
        return false;
    }
    GL_SAFEPOINT;
    glGenBuffers(1, &model->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) length, buffer, GL_STATIC_DRAW);
    *size = length;
    free(buffer);
    model->drawMode = GL_TRIANGLES;
    GL_RETURN(true, false, "loadModelDrawData failed: %x", error);
}

static void assignTextureUnit(GLenum textureUnit, GLuint samplerIndex, const texture_t texture) {
    GL_SAFEPOINT;
    LOGI("TMU assign: %i %i (%x %i)", textureUnit - GL_TEXTURE0, samplerIndex, texture.target, texture.name);
    glActiveTexture(textureUnit);
    glBindTexture(texture.target, texture.name);
    glUniform1i(samplerIndex, textureUnit - GL_TEXTURE0);
}

static void initDepthOutput(XrExtent2Di depthExtent, uint32_t nViews, GLuint depthOutput) {
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthOutput);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, depthExtent.width, depthExtent.height, nViews, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
}

#ifdef DEBUG
static void debugCallback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam) {
    LOGI("GL debug(%x %x %i %x): %s", source, type, id, severity, message);
}

static void initDebugOutput() {
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(debugCallback, NULL);
}
#endif

static bool internalInitRenderer(AAssetManager *assetManager) {
#ifdef DEBUG
    initDebugOutput();
#endif
    checkOVRMultiview();
    if(!lm_model_render_program_create(&rs.worldProgram)) return false;
    if(!rendertarget_blit_program_create(&rs.blitProgram)) return false;
    if(!singlecolor_program_create(&rs.singlecolorProgram)) return false;

    assert(rs.worldProgram.u.matrixBlockBinding == rs.blitProgram.u.matrixBlockBinding);
    if(!mv.hasMultiview) {
        assert(rs.worldProgram.u.projectionIndex == rs.blitProgram.u.projectionIndex);
    }

    if(!createSurfaceTextureId()) return false;
    if(!loadTextures(assetManager)) return false;
    {
        asset_info_t modelAssetInfo = {assetManager, "simplemodel.x"};
        off64_t modelLength = 0;
        if(!loadModelDrawData(&rs.worldModel, &modelAssetInfo, &modelLength)) return false;
        if(!initWorldModelDrawLayout(&rs.worldModel, (size_t) modelLength, rs.worldProgram)) return false;
    }
    {
        asset_info_t modelAssetInfo = {assetManager, "tv.x"};
        off64_t modelLength = 0;
        if(!loadModelDrawData(&rs.targetRectModel, &modelAssetInfo, &modelLength)) return false;
        if(!initTvModelDrawLayout(&rs.targetRectModel, (size_t) modelLength, rs.blitProgram)) return false;
    }
    createLineModel(&rs.leftControllerRay);
    createLineModel(&rs.rightControllerRay);
    GL_SAFEPOINT;

    const float mat_array[3*16*sizeof(GLfloat)];
    glGenBuffers(1, &rs.matrixBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, rs.matrixBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(mat_array), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, rs.matrixBuffer);

    glUseProgram(rs.worldProgram.name);
    assignTextureUnit(GL_TEXTURE0, rs.worldProgram.u.textureSampler, rs.atlas);
    glTexParameteri(rs.atlas.target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(rs.atlas.target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    assignTextureUnit(GL_TEXTURE1, rs.worldProgram.u.lightingSampler, rs.light);


    glUseProgram(rs.blitProgram.name);
    assignTextureUnit(GL_TEXTURE2, rs.blitProgram.u.rtSampler, rs.surface);

    // Set up framebuffer
    glGenFramebuffers(1, &rs.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, rs.framebuffer);
    glGenTextures(1, &rs.depthOutput);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 1, 0, 1);
    glLineWidth(4);
    GL_RETURN(true, false, "internalInitRenderer failed: %x", error);
}

bool initRenderer(AAssetManager *assetManager) {
    if(!initOpenGLES()) return false;
    if(!makeContextCurrent()) goto destroy_gles;
    if(!internalInitRenderer(assetManager)) goto destroy_gles;
    return true;
    destroy_gles:
    destroyOpenGLES();
    return false;
}

static void resizeDepthBuffers(const XrExtent2Di newExtent) {
    LOGI("Depth buffer size change");
    initDepthOutput(newExtent, mv.hasMultiview ? xrinfo.nViews : 1, rs.depthOutput);
    if(mv.hasMultiview) {
        mv.FramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rs.depthOutput, 0, 0, xrinfo.nViews);
    } else {
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rs.depthOutput, 0, 0);
    }
    rs.depthSize = newExtent;
}

// this is for ease of access for when you need the model matrix to calculate the controller ray but don't need the projection or view
static void calculateModelMatrix(XrMatrix4x4f* modelOut) {
    XrMatrix4x4f model_translate, model_rotate, model;
    XrMatrix4x4f_CreateTranslation(&model_translate, 1.5f, -2, -15.5f);
    XrMatrix4x4f_CreateRotation(&model_rotate, 0, 180, 0);
    XrMatrix4x4f_Multiply(&model, &model_rotate, &model_translate);
    memcpy(&modelOut[0], &model.m, sizeof(float[16]));
}

static void calculateProjectionViewMatrices(frame_begin_end_state_t *state, XrMatrix4x4f* modelOut) {
    float matrixBuffer[3 * 16 * sizeof(GLfloat)];
    for(uint32_t i = 0; i < xrinfo.nViews; i++) {
        XrMatrix4x4f projection, view, pv;

        XrCompositionLayerProjectionView projectionView = state->projectionViews[i];

        XrPosef pose = projectionView.pose;

        XrFovf projectionFov = projectionView.fov;

        XrMatrix4x4f_CreateIdentity(&pv);
        XrMatrix4x4f_CreateProjectionFov(&projection, projectionFov, 0.1f, 1000);
        XrMatrix4x4f_CreateViewMatrix(&view, &pose.position, &pose.orientation);

        XrMatrix4x4f_Multiply(&pv, &projection, &view);
        memcpy(&matrixBuffer[16*i], &pv.m, sizeof(float[16]));
    }
    {
        XrMatrix4x4f model;
        calculateModelMatrix(&model);
        memcpy(&modelOut[0], &model.m, sizeof(float[16]));
        memcpy(&matrixBuffer[16*xrinfo.nViews], &model.m, sizeof(float[16]));
    }
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(matrixBuffer), matrixBuffer);
}

static void drawModel(const model_t model, GLuint projIndex) {
    glUseProgram(model.program);
    if(projIndex != -1) glUniform1ui(rs.worldProgram.u.projectionIndex, projIndex);
    glBindVertexArray(model.vao);
    glDrawArrays(model.drawMode, 0, model.elementCount);
}

static void drawPass(GLuint projIndex) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawModel(rs.worldModel, projIndex);
    drawModel(rs.targetRectModel, projIndex);
    drawModel(rs.leftControllerRay, projIndex);
    drawModel(rs.rightControllerRay, projIndex);
}

GLuint getRenderTargetName() {
    return rs.surface.name;
}

void renderFrame(frame_begin_end_state_t *state) {
    XrOffset2Di offset = state->frame.outputRect.offset;
    XrExtent2Di extent = state->frame.outputRect.extent;
    GLuint targetColorTexture = xrinfo.renderTarget.swapchainTextures[state->frame.imageIndex];

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rs.framebuffer);

    XrMatrix4x4f model;
    calculateProjectionViewMatrices(state, &model);

    if((offset.x + extent.width) != rs.depthSize.width || (offset.y + extent.height) != rs.depthSize.height) {
        XrExtent2Di depthExtent = {(offset.x + extent.width), (offset.y + extent.height)};
        resizeDepthBuffers(depthExtent);
    }

    {
        for (int i = 0; i < 2; i++) {
            model_t* line = i == 0 ? &rs.leftControllerRay : &rs.rightControllerRay;
            XrVector3f start, end;
            getControllerRay(i, model, &start, &end);

            XrVector3f lineColor = {1, 0, 0};
            uploadLineModelData(line, lineColor, start, end);

            XrVector3f color = {1, 0, 1};
            XrVector3f intersection, result;
            XrVector3f_Sub(&result, &end, &start);
            XrVector3f_Normalize(&result);
            if (rayIntersectsScreen(start, result, &intersection)) {
                uploadLineModelData(line, color, start, intersection);
            }
        }
    }


    glViewport(offset.x, offset.y, extent.width, extent.height);

    if(!mv.hasMultiview){
        for(uint32_t i = 0; i < xrinfo.nViews; i++) {
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, targetColorTexture, 0, i);
            drawPass(i);
        }
    } else {
        mv.FramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, targetColorTexture, 0, 0, xrinfo.nViews);
        drawPass(-1);
    }
}