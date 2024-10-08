
#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) flat in uint texture_id;

layout(binding = 1) uniform sampler2D textures[];

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_emissive;

void main() {
    vec4 tex_color = texture(textures[nonuniformEXT(texture_id)], frag_uv);
    tex_color *= frag_color;

    out_color = tex_color;
}
