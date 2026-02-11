#version 450
#extension GL_GOOGLE_include_directive : require

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1
#include "smaa_common.glsl"

layout(set = 0, binding = 1) uniform sampler2DArray edgesTex;
layout(set = 0, binding = 2) uniform sampler2D areaTex;
layout(set = 0, binding = 3) uniform sampler2D searchTex;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 pixCoord;
layout(location = 2) in vec4 offset[3];
layout(location = 0) out vec4 outWeight; // R8G8B8A8

void main() {
    outWeight = SMAABlendingWeightCalculationPS(texCoord, pixCoord, offset,
                                                edgesTex, areaTex, searchTex, vec4(0));
}