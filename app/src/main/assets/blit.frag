#version 450
layout(location = 0) in vec2 tex_coord;
layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 2) uniform sampler2D render_source;

vec4 to_linear(vec4 srgb) {
    bvec3 cutoff = lessThan(srgb.rgb, vec3(0.04045));
    vec3 higher = pow((srgb.rgb + vec3(0.055))/1.055, vec3(2.4));
    vec3 lower = srgb.rgb/12.92;
    return vec4(mix(higher, lower, cutoff), srgb.a);
}

void main() {
    vec2 tex_inverted = vec2(tex_coord.x, (tex_coord.y * -1.0) + 1.0);
    fragColor = to_linear(vec4(texture(render_source, tex_inverted).xyz, 1.0));
}