
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 in_position_size;
layout(location = 1) in vec4 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_rotation_material;

layout(location = 1) out vec2 frag_uv;
layout(location = 2) out vec4 frag_color;
layout(location = 3) flat out uint out_texture_id;

layout(push_constant) uniform PushConstants {
    mat4 view_proj;
} push_constants;

vec2 rotate(vec2 v, float a) {
    float s = sin(a);
    float c = cos(a);

    mat2 m = mat2(c, -s, s, c);

    return m * v;
}

vec2 positions[4] = vec2[4](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));

void main() {
    const uint vertex_idx = gl_VertexIndex % 6;
    const uint corner_index = vertex_idx > 2 ? (vertex_idx - 1) % 4 : vertex_idx;

    vec2 pos = positions[corner_index];
    pos = rotate(pos, in_rotation_material.x);
    pos *= in_position_size.zw;
    pos += in_position_size.xy;

    gl_Position = push_constants.view_proj * vec4(pos, 0.0, 1.0);

    vec2 uvs[4] = vec2[4](vec2(in_uv.x, in_uv.y), vec2(in_uv.z, in_uv.y), vec2(in_uv.z, in_uv.w), vec2(in_uv.x, in_uv.w));

    frag_uv = uvs[corner_index];
    frag_color = in_color;
    out_texture_id = uint(in_rotation_material.y);
}
