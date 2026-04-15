#pragma once

#include <entt/entity/entity.hpp>
#include "Core/Memory.h"
#include "Graphics/Camera.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Life
{
    namespace Assets
    {
        class TextureAsset;
    }

    struct IdComponent
    {
        std::string Id;
    };

    struct TagComponent
    {
        std::string Tag = "Entity";
        bool Enabled = true;
    };

    struct TransformComponent
    {
        glm::vec3 LocalPosition{ 0.0f, 0.0f, 0.0f };
        glm::vec3 LocalRotation{ 0.0f, 0.0f, 0.0f };
        glm::vec3 LocalScale{ 1.0f, 1.0f, 1.0f };
    };

    struct HierarchyComponent
    {
        entt::entity Parent{ entt::null };
        std::vector<entt::entity> Children;
    };

    struct CameraComponent
    {
        ProjectionType Projection = ProjectionType::Perspective;
        float PerspectiveFieldOfView = 60.0f;
        float PerspectiveNearClip = 0.1f;
        float PerspectiveFarClip = 1000.0f;
        float OrthographicSize = 5.0f;
        float OrthographicNearClip = -1.0f;
        float OrthographicFarClip = 1.0f;
        int32_t Priority = 0;
        bool Primary = false;
        CameraClearMode ClearMode = CameraClearMode::SolidColor;
        glm::vec4 ClearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
        Viewport ViewportRect{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
    };

    struct SpriteComponent
    {
        glm::vec2 Size{ 1.0f, 1.0f };
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::string TextureAssetKey;
        Ref<Assets::TextureAsset> TextureAsset;
    };
}
