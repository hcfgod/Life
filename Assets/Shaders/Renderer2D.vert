#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform SceneConstants
{
    mat4 uViewProjection;
} sceneConstants;

layout(location = 0) in vec2 inLocalPosition;
layout(location = 1) in vec2 inLocalTexCoord;
layout(location = 2) in vec4 inQuadCenter;
layout(location = 3) in vec4 inQuadXAxis;
layout(location = 4) in vec4 inQuadYAxis;
layout(location = 5) in vec4 inColor;
layout(location = 6) in vec4 inTexRect;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main()
{
    vec3 worldPosition = inQuadCenter.xyz
        + inQuadXAxis.xyz * inLocalPosition.x
        + inQuadYAxis.xyz * inLocalPosition.y;

    gl_Position = sceneConstants.uViewProjection * vec4(worldPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = mix(inTexRect.xy, inTexRect.zw, inLocalTexCoord);
}
