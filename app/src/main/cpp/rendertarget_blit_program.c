//
// Created by maks on 12.12.2024.
//

#include "rendertarget_blit_program.h"
#include "gles_shader.h"
#include "gl_safepoint.h"
#include "multiview_detect.h"
#include <alloca.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

static const char* vertexProgramEntry =
                                    "#version 300 es\n";

static const char* vertexProgramOVRMultiview =
                                    "#extension GL_OVR_multiview2 : enable\n"
                                    "layout(num_views = 2) in;\n"
                                    "#define PVIndex (gl_ViewID_OVR)\n";

static const char* vertexProgramDefault =
                                    "layout(location = 3) uniform uint PVIndex;";

static const char* vertexProgram = "layout(std140, location = 0) uniform MatrixBlock {"
                                   "   mat4 PV[2];"
                                   "   mat4 M;"
                                   "};"
                                   "in vec3 vertPos;"
                                   "in vec2 vertTex;"
                                   "out vec2 texCoord;"
                                   "void main() {"
                                   "   gl_Position = (PV[PVIndex] * M) * vec4(vertPos, 1.0);"
                                   "   texCoord = vertTex;"
                                   "}";
// We need to convert from sRGB to linear here because it appears that the external sampler uses the
// sRGB color space, while the fragment color output is linear (converted back into sRGB by the GLES implementation
static const char* fragmentProgram = "#version 300 es\n"
                                     "#extension GL_OES_EGL_image_external_essl3 : enable\n"
                                     "layout(location = 6) uniform samplerExternalOES renderSource;"
                                     "layout(location = 0) out vec4 fragColor;"
                                     "in vec2 texCoord;"
                                     "vec4 toLinear(vec4 sRGB)\n"
                                     "{\n"
                                     "    bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));\n"
                                     "    vec3 higher = pow((sRGB.rgb + vec3(0.055))/vec3(1.055), vec3(2.4));\n"
                                     "    vec3 lower = sRGB.rgb/vec3(12.92);\n"
                                     "\n"
                                     "    return vec4(mix(higher, lower, cutoff), sRGB.a);\n"
                                     "}"
                                     "void main() {"
                                     "   vec2 texInverted = vec2(texCoord.x, (texCoord.y * -1.0) + 1.0);"
                                     "   fragColor = toLinear(vec4(texture(renderSource, texInverted).xyz, 1));"
                                     "}";

bool rendertarget_blit_program_create(rendertarget_blit_render_program_t* program) {
    GL_SAFEPOINT;
    GLuint programId;
    shader_source_t* sources = alloca(sizeof(shader_source_t) + sizeof(shader_source_t) + sizeof(const char*) * 2);
    sources[0].pipelineStage = GL_FRAGMENT_SHADER;
    sources[0].shaderSources[0] = fragmentProgram;
    sources[0].sourceCount = 1;
    sources[1].pipelineStage = GL_VERTEX_SHADER;
    sources[1].shaderSources[0] = vertexProgramEntry;
    //Ignore inspection since we allocate two additional char* pointers for this specific purpose above
#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArrayIndexOutOfBounds"
    sources[1].shaderSources[1] = mv.hasMultiview ? vertexProgramOVRMultiview : vertexProgramDefault;
    sources[1].shaderSources[2] = vertexProgram;
#pragma clang diagnostic pop
    sources[1].sourceCount = 3;

    if(!createProgram(&programId, 2, sources)) return false;
    GLuint blockIndex = glGetUniformBlockIndex(programId, "MatrixBlock");
    glUniformBlockBinding(programId, blockIndex, 0);
    program->u.matrixBlockBinding = 0;
    program->u.rtSampler = glGetUniformLocation(programId, "renderSource");
    if(!mv.hasMultiview) {
        program->u.projectionIndex = glGetUniformLocation(programId, "PVIndex");
    }
    program->v.position = glGetAttribLocation(programId, "vertPos");
    program->v.texCoord = glGetAttribLocation(programId, "vertTex");
    program->name = programId;
    GL_RETURN(true, false, "rendertarget_blit creation failed: %x", error);
}