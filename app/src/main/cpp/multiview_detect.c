//
// Created by maks on 09.12.2024.
//

#include "multiview_detect.h"
#include <string.h>
#include <EGL/egl.h>

multiview_info_t mv = {NULL, false};

static bool hasMultiviewExtension() {
    const GLubyte * extensions = glGetString(GL_EXTENSIONS);
    if(strstr((const char*)extensions, "GL_OVR_multiview") == NULL) return false;
    return true;
}

void checkOVRMultiview() {
    if(!hasMultiviewExtension()) return;
    GLint numViews = 0;
    glGetIntegerv(GL_MAX_VIEWS_OVR, &numViews);
    if(numViews < 2) return;
    mv.hasMultiview = true;
    mv.FramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR) eglGetProcAddress("glFramebufferTextureMultiviewOVR");
}