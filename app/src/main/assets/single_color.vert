#version 450
#extension GL_EXT_multiview : enable

layout(set = 0, binding = 0) uniform MatrixBlock {
    mat4 projection_views[2];
    mat4 model;
} ubo;

layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec3 vertex_color;

layout(location = 0) out vec3 frag_color;

void main() {
    gl_Position = (ubo.projection_views[gl_ViewIndex] * ubo.model) * vec4(vertex_pos, 1.0);
    gl_Position.y = -gl_Position.y;
    frag_color = vertex_color;
}