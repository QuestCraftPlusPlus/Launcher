#define SMAA_PRESENT_MEDIUM
#define SMAA_GLSL_4

layout(push_constant) uniform Constants {
    vec4 rt_metrics;
} c;
#define SMAA_RT_METRICS c.rt_metrics

#extension GL_EXT_multiview : enable
vec4 smaaSample(sampler2DArray tex, vec2 coord) {
    return texture(tex, vec3(coord, gl_ViewIndex));
}
vec4 smaaSample(sampler2D tex, vec2 coord) {
    return texture(tex, coord);
}

#define SMAA_TEXTURE2D_SAMPLE(tex, coord) smaaSample(tex, coord)
#define SMAA_TEXTURE2D_SAMPLE_LEVEL0(tex, coord) smaaSample(tex, coord)
#define SMAA_TEXTURE2D_SAMPLE_OFFSET(tex, coord, offset) smaaSample(tex, coord + offset * c.rt_metrics.xy)
#include "smaa.hlsl"