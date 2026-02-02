#version 450
#extension GL_EXT_multiview : enable

layout(set = 0, binding = 0) uniform MatrixBlock {
    mat4 projection_views[2];
    mat4 model;
} ubo;

layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec2 vertex_uv;

layout(location = 0) out vec2 tex_coord;

void main() {
    vec3 pos = vertex_pos;
//    pos.y = -pos.y;
    gl_Position = (ubo.projection_views[gl_ViewIndex] * ubo.model) * vec4(pos, 1.0);
    gl_Position.y = -gl_Position.y;
    tex_coord = vertex_uv;
}