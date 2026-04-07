#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(set = 0, binding = 1) uniform texture2D uTexture;
layout(set = 0, binding = 2) uniform sampler uTextureSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sampler2D(uTexture, uTextureSampler), fragTexCoord) * fragColor;
}
