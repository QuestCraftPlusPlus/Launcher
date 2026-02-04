#version 450
layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform sampler2D texture_base_color;
layout(set = 1, binding = 1) uniform sampler2D texture_metallic_roughness;

vec4 textureAniso(sampler2D tex, vec2 uv) {
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);

    float deltaMaxSqr = max(dot(dx, dx), dot(dy, dy));
    float deltaMinSqr = min(dot(dx, dx), dot(dy, dy));

    vec4 color = texture(tex, uv);
    color += texture(tex, uv + dx * 0.25);
    color += texture(tex, uv - dx * 0.25);
    color += texture(tex, uv + dy * 0.25);
    color += texture(tex, uv - dy * 0.25);

    return color / 5.0;
}

void main() {
    vec4 tex_color = textureAniso(texture_base_color, frag_uv);

    if(tex_color.a < 0.1) discard;

    frag_color = vec4(tex_color.rgb, tex_color.a);
}