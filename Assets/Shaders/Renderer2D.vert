#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform SceneConstants
{
    mat4 uViewProjection;
} sceneConstants;

layout(location = 0) in vec2 inLocalPosition;
layout(location = 1) in vec2 inLocalTexCoord;
layout(location = 2) in vec4 inQuadTransform;
layout(location = 3) in vec2 inQuadSize;
layout(location = 4) in vec4 inColor;
layout(location = 5) in vec4 inTexRect;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
    float sineRotation = sin(inQuadTransform.w);
    float cosineRotation = cos(inQuadTransform.w);
    mat2 rotation = mat2(cosineRotation, -sineRotation, sineRotation, cosineRotation);
    vec2 worldOffset = rotation * (inLocalPosition * inQuadSize);
    vec4 worldPosition = vec4(inQuadTransform.xyz + vec3(worldOffset, 0.0), 1.0);

    gl_Position = sceneConstants.uViewProjection * worldPosition;
    fragColor = inColor;
    fragTexCoord = mix(inTexRect.xy, inTexRect.zw, inLocalTexCoord);
}
