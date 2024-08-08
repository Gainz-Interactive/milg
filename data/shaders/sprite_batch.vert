
#version 450

layout(location = 0) in vec4 in_position_size;
layout(location = 1) in vec4 in_uv;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_rotation_emission;
layout(location = 4) in vec2 in_texture_indices;

layout(location = 1) out vec2 frag_uv;
layout(location = 2) out vec4 frag_color;
layout(location = 3) out float out_emission_strength;
layout(location = 4) flat out uint out_texture_id;
layout(location = 5) flat out uint out_emissive_texture_id;

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
    pos *= in_position_size.zw;
    pos = rotate(pos, in_rotation_emission.x);

    pos += in_position_size.xy;

    gl_Position = push_constants.view_proj * vec4(pos, 0.0, 1.0);

    vec2 uvs[4] = vec2[4](vec2(in_uv.x, in_uv.y), vec2(in_uv.z, in_uv.y), vec2(in_uv.z, in_uv.w), vec2(in_uv.x, in_uv.w));

    frag_uv = uvs[corner_index];
    frag_color = in_color;
    out_emission_strength = in_rotation_emission.y;
    out_texture_id = uint(in_texture_indices.x);
    out_emissive_texture_id = uint(in_texture_indices.y);
}
