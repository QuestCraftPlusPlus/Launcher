#version 450
#extension GL_GOOGLE_include_directive : require

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0
#include "smaa_common.glsl"

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec2 pixCoord;
layout(location = 2) out vec4 offset[3];

void main() {
    texCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);

    SMAABlendingWeightCalculationVS(texCoord, pixCoord, offset);
}