#version 450

layout(location = 0) in vec3 in_color;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
    vec3 tint;
    float size;
} constants;

void main() {
    out_color = vec4(in_color * constants.tint, 1.0f);
}
