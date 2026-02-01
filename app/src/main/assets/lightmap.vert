#version 450
#extension GL_EXT_multiview : enable

layout(set = 0, binding = 0) uniform MatrixBlock {
    mat4 projection_views[2];
    mat4 model;
} ubo;

layout(location = 0) in vec3 vertex_pos;
layout(location = 1) in vec4 vertex_texture_light;

layout(location = 0) out vec4 texture_light_coord;

void main() {
    gl_Position = (ubo.projection_views[gl_ViewIndex] * ubo.model) * vec4(vertex_pos, 1.0);
    gl_Position.y = -gl_Position.y;
    texture_light_coord = vertex_texture_light;
}