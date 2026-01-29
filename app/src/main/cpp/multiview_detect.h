//
// Created by maks on 09.12.2024.
//


#ifndef QCXR_MULTIVIEW_DETECT_H
#define QCXR_MULTIVIEW_DETECT_H

#include <GLES3/gl3.h>
#include <stdbool.h>

// Macros here are mostly stored for reference, so keep the IDE from giving warnings about them

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedMacroInspection"

#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR               0x9630
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR         0x9632
#define GL_MAX_VIEWS_OVR 0x9631
#define GLFRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR 0x9633

#pragma clang diagnostic pop

typedef void (*PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)( GLenum target, GLenum attachment,
                                                  GLuint texture, GLint level,
                                                  GLint baseViewIndex, GLsizei numViews );
typedef struct {
    PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR FramebufferTextureMultiviewOVR;
    bool hasMultiview;
} multiview_info_t;

extern multiview_info_t mv;

void checkOVRMultiview();

#endif //QCXR_MULTIVIEW_DETECT_H