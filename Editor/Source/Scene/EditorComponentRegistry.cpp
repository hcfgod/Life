#include "Editor/Scene/EditorComponentRegistry.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <utility>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
        #if __has_include(<imgui.h>)
            enum class AssetPickerKind
            {
                Texture
            };

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

            bool DrawStackedTextField(const char* label, const char* inputId, std::string& value)
            {
                ImGui::TextUnformatted(label);
                ImGui::SetNextItemWidth(-1.0f);
                return InputTextString(inputId, value);
            }

            std::string ToLowerAscii(std::string value)
            {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
                    {
                        return static_cast<char>(std::tolower(character));
                    });
                return value;
            }

            bool MatchesAssetPickerKind(const std::filesystem::path& relativePath, AssetPickerKind kind)
            {
                const std::string extension = ToLowerAscii(relativePath.extension().string());
                switch (kind)
                {
                case AssetPickerKind::Texture:
                    return extension == ".png" ||
                        extension == ".jpg" ||
                        extension == ".jpeg" ||
                        extension == ".bmp" ||
                        extension == ".tga" ||
                        extension == ".ppm" ||
                        extension == ".pgm";

                default:
                    return false;
                }
            }

            std::string MakeAssetKey(const std::filesystem::path& relativePath)
            {
                if (relativePath.empty())
                    return "Assets";

                return "Assets/" + relativePath.generic_string();
            }

            std::string GetAssetPickerPreviewLabel(const std::string& assetKey)
            {
                if (assetKey.empty())
                    return "<None>";

                const std::filesystem::path path(assetKey);
                const std::string fileName = path.filename().string();
                return fileName.empty() ? assetKey : fileName;
            }

            std::vector<std::string> CollectProjectAssetKeys(const EditorServices& services, AssetPickerKind kind)
            {
                std::vector<std::string> assetKeys;
                if (!services.ProjectService || !services.ProjectService->get().HasActiveProject())
                    return assetKeys;

                const std::filesystem::path assetsDirectory = services.ProjectService->get().GetActiveProject().Paths.AssetsDirectory;
                std::error_code ec;
                if (!std::filesystem::exists(assetsDirectory, ec) || !std::filesystem::is_directory(assetsDirectory, ec))
                    return assetKeys;

                for (std::filesystem::recursive_directory_iterator it(assetsDirectory, std::filesystem::directory_options::skip_permission_denied, ec), end;
                    it != end;
                    it.increment(ec))
                {
                    if (ec)
                        break;

                    if (it->is_directory(ec))
                    {
                        ec.clear();
                        continue;
                    }
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    const std::filesystem::path relativePath = std::filesystem::relative(it->path(), assetsDirectory, ec).lexically_normal();
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }

                    if (!MatchesAssetPickerKind(relativePath, kind))
                        continue;

                    assetKeys.push_back(MakeAssetKey(relativePath));
                }

                std::sort(assetKeys.begin(), assetKeys.end(), [](const std::string& left, const std::string& right)
                    {
                        return ToLowerAscii(left) < ToLowerAscii(right);
                    });
                return assetKeys;
            }

            bool TryAcceptProjectAssetDrop(std::string& assetKey, AssetPickerKind kind)
            {
                if (!ImGui::BeginDragDropTarget())
                    return false;

                bool changed = false;
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                {
                    const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                    if (assetPayload != nullptr &&
                        assetPayload->Kind == ProjectAssetPayloadKind::File &&
                        assetPayload->RelativePath[0] != '\0')
                    {
                        const std::filesystem::path relativePath(assetPayload->RelativePath.data());
                        if (MatchesAssetPickerKind(relativePath, kind))
                        {
                            const std::string droppedAssetKey = MakeAssetKey(relativePath);
                            if (assetKey != droppedAssetKey)
                            {
                                assetKey = droppedAssetKey;
                                changed = true;
                            }
                        }
                    }
                }

                ImGui::EndDragDropTarget();
                return changed;
            }

            bool DrawProjectAssetPicker(const char* label, const char* pickerId, const EditorServices& services, std::string& assetKey, AssetPickerKind kind)
            {
                bool changed = false;
                ImGui::TextUnformatted(label);
                ImGui::PushID(pickerId);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##AssetPicker", GetAssetPickerPreviewLabel(assetKey).c_str()))
                {
                    const bool isNoneSelected = assetKey.empty();
                    if (ImGui::Selectable("<None>", isNoneSelected))
                    {
                        if (!isNoneSelected)
                        {
                            assetKey.clear();
                            changed = true;
                        }
                    }
                    if (isNoneSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }

                    const std::vector<std::string> assetKeys = CollectProjectAssetKeys(services, kind);
                    for (const std::string& candidate : assetKeys)
                    {
                        const bool selected = candidate == assetKey;
                        if (ImGui::Selectable(candidate.c_str(), selected))
                        {
                            if (!selected)
                            {
                                assetKey = candidate;
                                changed = true;
                            }
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
                changed |= TryAcceptProjectAssetDrop(assetKey, kind);
                if (!assetKey.empty())
                    ImGui::TextDisabled("%s", assetKey.c_str());
                ImGui::PopID();
                return changed;
            }

            bool ApplySpriteTextureAssetSelection(Life::SpriteComponent& sprite, const EditorServices& services, std::string textureAssetKey)
            {
                if (sprite.TextureAssetKey == textureAssetKey)
                    return false;

                sprite.TextureAssetKey = std::move(textureAssetKey);
                if (sprite.TextureAssetKey.empty())
                {
                    sprite.TextureAsset.reset();
                }
                else if (services.AssetManager)
                {
                    sprite.TextureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(sprite.TextureAssetKey);
                }
                else
                {
                    sprite.TextureAsset.reset();
                }
                return true;
            }

            template <typename TDrawFn>
            bool DrawLeftLabelRow(const char* rowId, const char* label, TDrawFn&& drawFn)
            {
                bool changed = false;
                ImGui::PushID(rowId);
                if (ImGui::BeginTable("##Row", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, 92.0f);
                    ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-1.0f);
                    changed = drawFn();
                    ImGui::EndTable();
                }
                ImGui::PopID();
                return changed;
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
                if (DrawStackedTextField("Tag", "##Tag", value))
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
                changed |= DrawLeftLabelRow("PositionRow", "Position", [&]()
                    {
                        return ImGui::DragFloat3("##Value", &transform.LocalPosition.x, 0.1f);
                    });
                changed |= DrawLeftLabelRow("RotationRow", "Rotation", [&]()
                    {
                        return ImGui::DragFloat3("##Value", &transform.LocalRotation.x, 0.05f);
                    });
                changed |= DrawLeftLabelRow("ScaleRow", "Scale", [&]()
                    {
                        return ImGui::DragFloat3("##Value", &transform.LocalScale.x, 0.05f);
                    });
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
                changed |= DrawLeftLabelRow("SizeRow", "Size", [&]()
                    {
                        return ImGui::DragFloat2("##Value", &sprite.Size.x, 0.05f, 0.01f, 100.0f);
                    });
                ImGui::Spacing();
                ImGui::TextUnformatted("Color");
                ImGui::SetNextItemWidth(-1.0f);
                changed |= ImGui::ColorEdit4("##Color", &sprite.Color.x);
                ImGui::Spacing();

                std::string textureKey = sprite.TextureAssetKey;
                changed |= DrawProjectAssetPicker("Texture Asset", "TextureAsset", services, textureKey, AssetPickerKind::Texture);
                changed |= ApplySpriteTextureAssetSelection(sprite, services, std::move(textureKey));

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
