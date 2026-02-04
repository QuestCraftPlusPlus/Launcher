#version 450
layout(location = 0) in vec4 texture_light_coord;
layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform sampler2D texture_atlas;
layout(set = 1, binding = 1) uniform sampler2D texture_lighting;

void main() {
    vec2 inverted_tex_coord = vec2(texture_light_coord.x, -texture_light_coord.y + 1.0);
    vec4 tex_color = texture(texture_atlas, inverted_tex_coord);

    if(tex_color.a < 0.1) discard;

    vec2 inverted_light_coord = vec2(texture_light_coord.z, -texture_light_coord.w + 1.0);
    float light_intensity = texture(texture_lighting, inverted_light_coord).r;

    frag_color = vec4(tex_color.rgb * light_intensity, tex_color.a);
}