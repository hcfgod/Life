#include "Editor/Scene/EditorComponentRegistry.h"

#include <algorithm>
#include <array>
#include <cstring>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
#if __has_include(<imgui.h>)
        bool InputTextString(const char* label, std::string& value)
        {
            std::array<char, 1024> buffer{};
            const std::size_t copyLength = std::min(value.size(), buffer.size() - 1);
            std::memcpy(buffer.data(), value.data(), copyLength);
            buffer[copyLength] = '\0';

            if (!ImGui::InputText(label, buffer.data(), buffer.size()))
                return false;

            value = buffer.data();
            return true;
        }
#endif

        EditorComponentDescriptor MakeTagDescriptor()
        {
            EditorComponentDescriptor descriptor;
            descriptor.Id = "tag";
            descriptor.DisplayName = "Tag";
            descriptor.Removable = false;
            descriptor.HasComponent = [](const Life::Entity& entity) { return entity.HasComponent<Life::TagComponent>(); };
            descriptor.CanAddComponent = [](const Life::Entity&) { return false; };
            descriptor.AddComponent = [](Life::Entity&) {};
            descriptor.RemoveComponent = [](Life::Entity&) { return false; };
            descriptor.DrawInspector = [](Life::Entity& entity, const EditorServices&) -> bool
            {
#if __has_include(<imgui.h>)
                Life::TagComponent& tag = entity.GetComponent<Life::TagComponent>();
                std::string value = tag.Tag;
                if (InputTextString("Tag", value))
                {
                    tag.Tag = std::move(value);
                    return true;
                }
#else
                (void)entity;
#endif
                return false;
            };
            return descriptor;
        }

        EditorComponentDescriptor MakeTransformDescriptor()
        {
            EditorComponentDescriptor descriptor;
            descriptor.Id = "transform";
            descriptor.DisplayName = "Transform";
            descriptor.Removable = false;
            descriptor.HasComponent = [](const Life::Entity& entity) { return entity.HasComponent<Life::TransformComponent>(); };
            descriptor.CanAddComponent = [](const Life::Entity&) { return false; };
            descriptor.AddComponent = [](Life::Entity&) {};
            descriptor.RemoveComponent = [](Life::Entity&) { return false; };
            descriptor.DrawInspector = [](Life::Entity& entity, const EditorServices&) -> bool
            {
#if __has_include(<imgui.h>)
                bool changed = false;
                Life::TransformComponent& transform = entity.GetComponent<Life::TransformComponent>();
                changed |= ImGui::DragFloat3("Position", &transform.LocalPosition.x, 0.1f);
                changed |= ImGui::DragFloat3("Rotation", &transform.LocalRotation.x, 0.05f);
                changed |= ImGui::DragFloat3("Scale", &transform.LocalScale.x, 0.05f);
                return changed;
#else
                (void)entity;
                return false;
#endif
            };
            return descriptor;
        }

        EditorComponentDescriptor MakeSpriteDescriptor()
        {
            EditorComponentDescriptor descriptor;
            descriptor.Id = "sprite";
            descriptor.DisplayName = "Sprite";
            descriptor.Removable = true;
            descriptor.HasComponent = [](const Life::Entity& entity) { return entity.HasComponent<Life::SpriteComponent>(); };
            descriptor.CanAddComponent = [](const Life::Entity& entity) { return !entity.HasComponent<Life::SpriteComponent>(); };
            descriptor.AddComponent = [](Life::Entity& entity)
            {
                if (!entity.HasComponent<Life::SpriteComponent>())
                    entity.AddComponent<Life::SpriteComponent>();
            };
            descriptor.RemoveComponent = [](Life::Entity& entity)
            {
                return entity.RemoveComponent<Life::SpriteComponent>();
            };
            descriptor.DrawInspector = [](Life::Entity& entity, const EditorServices& services) -> bool
            {
#if __has_include(<imgui.h>)
                bool changed = false;
                Life::SpriteComponent& sprite = entity.GetComponent<Life::SpriteComponent>();
                changed |= ImGui::DragFloat2("Size", &sprite.Size.x, 0.05f, 0.01f, 100.0f);
                changed |= ImGui::ColorEdit4("Color", &sprite.Color.x);

                std::string textureKey = sprite.TextureAssetKey;
                if (InputTextString("Texture Asset", textureKey))
                {
                    sprite.TextureAssetKey = std::move(textureKey);
                    if (sprite.TextureAssetKey.empty())
                    {
                        sprite.TextureAsset.reset();
                    }
                    else if (services.AssetManager)
                    {
                        sprite.TextureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(sprite.TextureAssetKey);
                    }
                    changed = true;
                }

                if (!sprite.TextureAssetKey.empty() && services.AssetManager && !sprite.TextureAsset)
                    sprite.TextureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(sprite.TextureAssetKey);

                return changed;
#else
                (void)entity;
                (void)services;
                return false;
#endif
            };
            return descriptor;
        }

        EditorComponentDescriptor MakeHierarchyDescriptor()
        {
            EditorComponentDescriptor descriptor;
            descriptor.Id = "hierarchy";
            descriptor.DisplayName = "Hierarchy";
            descriptor.Removable = false;
            descriptor.HasComponent = [](const Life::Entity& entity) { return entity.HasComponent<Life::HierarchyComponent>(); };
            descriptor.CanAddComponent = [](const Life::Entity&) { return false; };
            descriptor.AddComponent = [](Life::Entity&) {};
            descriptor.RemoveComponent = [](Life::Entity&) { return false; };
            descriptor.DrawInspector = [](Life::Entity& entity, const EditorServices&) -> bool
            {
#if __has_include(<imgui.h>)
                const Life::Entity parent = entity.GetParent();
                ImGui::Text("Parent: %s", parent.IsValid() ? parent.GetTag().c_str() : "<None>");
                ImGui::Text("Children: %d", static_cast<int>(entity.GetChildren().size()));
#else
                (void)entity;
#endif
                return false;
            };
            return descriptor;
        }
    }

    EditorComponentRegistry& EditorComponentRegistry::Get()
    {
        static EditorComponentRegistry registry;
        return registry;
    }

    EditorComponentRegistry::EditorComponentRegistry()
    {
        Register(MakeTagDescriptor());
        Register(MakeTransformDescriptor());
        Register(MakeSpriteDescriptor());
        Register(MakeHierarchyDescriptor());
    }

    void EditorComponentRegistry::Register(EditorComponentDescriptor descriptor)
    {
        auto existing = std::find_if(
            m_Descriptors.begin(),
            m_Descriptors.end(),
            [&](const EditorComponentDescriptor& candidate)
            {
                return candidate.Id == descriptor.Id;
            });

        if (existing != m_Descriptors.end())
            *existing = std::move(descriptor);
        else
            m_Descriptors.push_back(std::move(descriptor));
    }

    const std::vector<EditorComponentDescriptor>& EditorComponentRegistry::GetDescriptors() const noexcept
    {
        return m_Descriptors;
    }
}
