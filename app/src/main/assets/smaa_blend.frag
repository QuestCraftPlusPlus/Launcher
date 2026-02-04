#version 450
#extension GL_GOOGLE_include_directive : require

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa_common.glsl"

layout(set = 0, binding = 0) uniform sampler2DArray colorTex;
layout(set = 0, binding = 4) uniform sampler2DArray blendTex;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec4 offset;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = SMAANeighborhoodBlendingPS(texCoord, offset, colorTex, blendTex);
}