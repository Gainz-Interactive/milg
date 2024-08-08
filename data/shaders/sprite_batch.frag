
#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 1) in vec2 frag_uv;
layout(location = 2) in vec4 frag_color;
layout(location = 3) in float emission_strength;
layout(location = 4) flat in uint texture_id;
layout(location = 5) flat in uint emissive_texture_id;

layout(binding = 1) uniform sampler2D textures[];

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_emissive;

void main() {
    vec4 tex_color = texture(textures[nonuniformEXT(texture_id)], frag_uv);
    tex_color *= frag_color;

    vec4 emissive_color = texture(textures[nonuniformEXT(emissive_texture_id)], frag_uv);
    emissive_color.rgb = emissive_color.rgb * exp(emission_strength);

    out_color = tex_color;
    out_emissive = emissive_color * frag_color;
}
