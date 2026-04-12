#pragma once

#include <entt/entity/entity.hpp>
#include "Core/Memory.h"

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

    struct SpriteComponent
    {
        glm::vec2 Size{ 1.0f, 1.0f };
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::string TextureAssetKey;
        Ref<Assets::TextureAsset> TextureAsset;
    };
}
