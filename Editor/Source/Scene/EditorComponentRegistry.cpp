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

            const char* ResolveProjectionLabel(Life::ProjectionType projection) noexcept
            {
                switch (projection)
                {
                    case Life::ProjectionType::Orthographic: return "Orthographic";
                    case Life::ProjectionType::Perspective:
                    default: return "Perspective";
                }
            }

            bool DrawProjectionCombo(const char* label, Life::ProjectionType& value)
            {
                constexpr std::array<std::pair<Life::ProjectionType, const char*>, 2> options{{
                    { Life::ProjectionType::Perspective, "Perspective" },
                    { Life::ProjectionType::Orthographic, "Orthographic" }
                }};

                bool changed = false;
                if (ImGui::BeginCombo(label, ResolveProjectionLabel(value)))
                {
                    for (const auto& option : options)
                    {
                        const bool selected = option.first == value;
                        if (ImGui::Selectable(option.second, selected) && !selected)
                        {
                            value = option.first;
                            changed = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                return changed;
            }

            const char* ResolveClearModeLabel(Life::CameraClearMode clearMode) noexcept
            {
                switch (clearMode)
                {
                    case Life::CameraClearMode::DepthOnly: return "Depth Only";
                    case Life::CameraClearMode::None: return "None";
                    case Life::CameraClearMode::SolidColor:
                    default: return "Solid Color";
                }
            }

            bool DrawClearModeCombo(const char* label, Life::CameraClearMode& value)
            {
                constexpr std::array<std::pair<Life::CameraClearMode, const char*>, 3> options{{
                    { Life::CameraClearMode::SolidColor, "Solid Color" },
                    { Life::CameraClearMode::DepthOnly, "Depth Only" },
                    { Life::CameraClearMode::None, "None" }
                }};

                bool changed = false;
                if (ImGui::BeginCombo(label, ResolveClearModeLabel(value)))
                {
                    for (const auto& option : options)
                    {
                        const bool selected = option.first == value;
                        if (ImGui::Selectable(option.second, selected) && !selected)
                        {
                            value = option.first;
                            changed = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                return changed;
            }

            bool ApplyPrimaryCameraSelection(Life::Entity& entity, bool primary)
            {
                Life::CameraComponent* camera = entity.TryGetComponent<Life::CameraComponent>();
                if (camera == nullptr)
                    return false;

                bool changed = camera->Primary != primary;
                camera->Primary = primary;
                if (primary)
                {
                    for (Life::Entity other : entity.GetScene().GetEntities())
                    {
                        if (other == entity)
                            continue;

                        if (Life::CameraComponent* otherCamera = other.TryGetComponent<Life::CameraComponent>())
                            otherCamera->Primary = false;
                    }
                }

                entity.GetScene().EnsureAtLeastOneCamera();
                return changed;
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

        EditorComponentDescriptor MakeCameraDescriptor()
        {
            EditorComponentDescriptor descriptor;
            descriptor.Id = "camera";
            descriptor.DisplayName = "Camera";
            descriptor.Removable = true;
            descriptor.HasComponent = [](const Life::Entity& entity) { return entity.HasComponent<Life::CameraComponent>(); };
            descriptor.CanAddComponent = [](const Life::Entity& entity) { return !entity.HasComponent<Life::CameraComponent>(); };
            descriptor.AddComponent = [](Life::Entity& entity)
            {
                if (entity.HasComponent<Life::CameraComponent>())
                    return;

                Life::CameraComponent camera;
                camera.Projection = Life::ProjectionType::Orthographic;
                camera.OrthographicSize = 5.0f;
                camera.OrthographicNearClip = 0.1f;
                camera.OrthographicFarClip = 100.0f;
                camera.PerspectiveNearClip = 0.1f;
                camera.PerspectiveFarClip = 1000.0f;
                camera.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };
                camera.Primary = !entity.GetScene().HasCamera();
                entity.AddComponent<Life::CameraComponent>(camera);
                entity.GetScene().EnsureAtLeastOneCamera();
            };
            descriptor.RemoveComponent = [](Life::Entity& entity)
            {
                return entity.RemoveComponent<Life::CameraComponent>();
            };
            descriptor.DrawInspector = [](Life::Entity& entity, const EditorServices&) -> bool
            {
#if __has_include(<imgui.h>)
                bool changed = false;
                Life::CameraComponent& camera = entity.GetComponent<Life::CameraComponent>();

                changed |= DrawLeftLabelRow("ProjectionRow", "Projection", [&]()
                    {
                        return DrawProjectionCombo("##Value", camera.Projection);
                    });

                bool primary = camera.Primary;
                if (ImGui::Checkbox("Primary Camera", &primary))
                    changed |= ApplyPrimaryCameraSelection(entity, primary);

                if (camera.Projection == Life::ProjectionType::Perspective)
                {
                    changed |= DrawLeftLabelRow("PerspectiveFovRow", "Field Of View", [&]()
                        {
                            return ImGui::DragFloat("##Value", &camera.PerspectiveFieldOfView, 0.25f, 1.0f, 179.0f);
                        });
                    changed |= DrawLeftLabelRow("PerspectiveNearRow", "Near Clip", [&]()
                        {
                            return ImGui::DragFloat("##Value", &camera.PerspectiveNearClip, 0.01f, 0.001f, 1000.0f);
                        });
                    changed |= DrawLeftLabelRow("PerspectiveFarRow", "Far Clip", [&]()
                        {
                            return ImGui::DragFloat("##Value", &camera.PerspectiveFarClip, 0.1f, 0.01f, 100000.0f);
                        });
                }
                else
                {
                    changed |= DrawLeftLabelRow("OrthographicSizeRow", "Size", [&]()
                        {
                            return ImGui::DragFloat("##Value", &camera.OrthographicSize, 0.05f, 0.01f, 100000.0f);
                        });
                    changed |= DrawLeftLabelRow("OrthographicNearRow", "Near Clip", [&]()
                        {
                            return ImGui::DragFloat("##Value", &camera.OrthographicNearClip, 0.01f, -100000.0f, 100000.0f);
                        });
                    changed |= DrawLeftLabelRow("OrthographicFarRow", "Far Clip", [&]()
                        {
                            return ImGui::DragFloat("##Value", &camera.OrthographicFarClip, 0.01f, -100000.0f, 100000.0f);
                        });
                }

                changed |= DrawLeftLabelRow("CameraPriorityRow", "Priority", [&]()
                    {
                        return ImGui::DragInt("##Value", &camera.Priority, 1.0f);
                    });
                changed |= DrawLeftLabelRow("CameraClearModeRow", "Clear Mode", [&]()
                    {
                        return DrawClearModeCombo("##Value", camera.ClearMode);
                    });

                if (camera.ClearMode == Life::CameraClearMode::SolidColor)
                {
                    ImGui::TextUnformatted("Clear Color");
                    ImGui::SetNextItemWidth(-1.0f);
                    changed |= ImGui::ColorEdit4("##CameraClearColor", &camera.ClearColor.x);
                }

                return changed;
#else
                (void)entity;
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
            Register(MakeCameraDescriptor());
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
