#version 450
layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform sampler2D texture_base_color;
layout(set = 1, binding = 1) uniform sampler2D texture_metallic_roughness;

void main() {
    vec4 tex_color = texture(texture_base_color, frag_uv);

    if(tex_color.a < 0.1) discard;

    frag_color = vec4(tex_color.rgb, tex_color.a);
}