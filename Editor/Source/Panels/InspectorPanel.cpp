#include "Editor/Panels/InspectorPanel.h"

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorComponentRegistry.h"

#include "Assets/TextureAssetImporter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

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

        void DrawPanelHeader(const char* title, const char* subtitle)
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "%s", title);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", subtitle);
            ImGui::Separator();
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        bool EndsWith(std::string_view value, std::string_view suffix)
        {
            return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
        }

        std::string MakeAssetKey(const std::filesystem::path& relativePath)
        {
            if (relativePath.empty())
                return "Assets";

            return "Assets/" + relativePath.generic_string();
        }

        std::string ResolveVisibleAssetName(const std::filesystem::path& relativePath)
        {
            const std::string fileName = relativePath.filename().string();
            const std::string lowerFileName = ToLowerAscii(fileName);
            constexpr std::array<std::string_view, 11> specialSuffixes{
                ".scene.json",
                ".material.json",
                ".prefab.json",
                ".tilemap.json",
                ".tileset.json",
                ".tile.json",
                ".tilepalette.json",
                ".animationclip.json",
                ".animation.json",
                ".anim.json",
                ".animcontroller.json"
            };

            for (const std::string_view suffix : specialSuffixes)
            {
                if (EndsWith(lowerFileName, suffix) && fileName.size() > suffix.size())
                    return fileName.substr(0, fileName.size() - suffix.size());
            }

            if (relativePath.has_extension())
                return relativePath.stem().string();

            return fileName;
        }

        bool IsTextureAssetPath(const std::filesystem::path& relativePath)
        {
            const std::string extension = ToLowerAscii(relativePath.extension().string());
            return extension == ".png" ||
                extension == ".jpg" ||
                extension == ".jpeg" ||
                extension == ".bmp" ||
                extension == ".tga" ||
                extension == ".hdr" ||
                extension == ".psd" ||
                extension == ".gif" ||
                extension == ".ppm" ||
                extension == ".pnm";
        }

        const char* ResolveTextureFilterLabel(Life::Assets::TextureFilter filter) noexcept
        {
            switch (filter)
            {
                case Life::Assets::TextureFilter::Nearest: return "Nearest";
                case Life::Assets::TextureFilter::Linear: return "Linear";
                case Life::Assets::TextureFilter::NearestMipmapNearest: return "Nearest Mipmap Nearest";
                case Life::Assets::TextureFilter::LinearMipmapLinear: return "Linear Mipmap Linear";
            }

            return "Linear";
        }

        const char* ResolveTextureWrapLabel(Life::Assets::TextureWrap wrap) noexcept
        {
            switch (wrap)
            {
                case Life::Assets::TextureWrap::Repeat: return "Repeat";
                case Life::Assets::TextureWrap::ClampToEdge: return "Clamp To Edge";
                case Life::Assets::TextureWrap::MirroredRepeat: return "Mirrored Repeat";
            }

            return "Repeat";
        }

        bool DrawTextureFilterCombo(const char* label, Life::Assets::TextureFilter& value)
        {
            constexpr std::array<std::pair<Life::Assets::TextureFilter, const char*>, 4> options{{
                { Life::Assets::TextureFilter::Nearest, "Nearest" },
                { Life::Assets::TextureFilter::Linear, "Linear" },
                { Life::Assets::TextureFilter::NearestMipmapNearest, "Nearest Mipmap Nearest" },
                { Life::Assets::TextureFilter::LinearMipmapLinear, "Linear Mipmap Linear" }
            }};

            bool changed = false;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo(label, ResolveTextureFilterLabel(value)))
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

        bool DrawTextureWrapCombo(const char* label, Life::Assets::TextureWrap& value)
        {
            constexpr std::array<std::pair<Life::Assets::TextureWrap, const char*>, 3> options{{
                { Life::Assets::TextureWrap::Repeat, "Repeat" },
                { Life::Assets::TextureWrap::ClampToEdge, "Clamp To Edge" },
                { Life::Assets::TextureWrap::MirroredRepeat, "Mirrored Repeat" }
            }};

            bool changed = false;
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo(label, ResolveTextureWrapLabel(value)))
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

        Life::Assets::TextureSpecification LoadTextureSpecification(const std::string& assetKey, const EditorServices& services)
        {
            if (services.AssetDatabase)
            {
                const auto recordResult = services.AssetDatabase->get().FindByKey(assetKey);
                if (recordResult.IsSuccess())
                    return Life::Assets::TextureSpecificationFromImporterSettingsJson(recordResult.GetValue().ImporterSettings);
            }

            if (services.AssetManager)
            {
                if (auto textureAsset = services.AssetManager->get().GetCachedByKey<Life::Assets::TextureAsset>(assetKey))
                    return textureAsset->GetSpecification();
            }

            return {};
        }

        void PersistTextureSpecification(const std::filesystem::path& relativePath,
                                         const Life::Assets::TextureSpecification& specification,
                                         const EditorServices& services,
                                         EditorSceneState& sceneState)
        {
            if (!services.AssetDatabase)
            {
                sceneState.SetStatusMessage("Texture settings could not be saved because AssetDatabase is unavailable.", true);
                return;
            }

            const std::string assetKey = MakeAssetKey(relativePath);
            const auto result = services.AssetDatabase->get().ImportOrUpdate(
                assetKey,
                Life::Assets::AssetImporter<Life::Assets::TextureAsset>::Type,
                Life::Assets::AssetImporter<Life::Assets::TextureAsset>::SettingsToJson(specification),
                Life::Assets::AssetImporter<Life::Assets::TextureAsset>::Version);
            if (result.IsFailure())
            {
                sceneState.SetStatusMessage(result.GetError().GetErrorMessage(), true);
                return;
            }

            const std::string displayName = ResolveVisibleAssetName(relativePath);
            if (services.AssetManager)
            {
                if (auto textureAsset = services.AssetManager->get().GetCachedByKey<Life::Assets::TextureAsset>(assetKey))
                {
                    textureAsset->ApplySpecification(specification);
                    if (!textureAsset->Reload())
                    {
                        sceneState.SetStatusMessage("Saved texture settings for '" + displayName + "', but failed to reload the cached texture.", true);
                        return;
                    }
                }
            }

            sceneState.SetStatusMessage("Saved texture settings for '" + displayName + "'.", false);
        }

        void RenderSelectedProjectAssetInspector(const std::filesystem::path& relativePath,
                                                 const EditorServices& services,
                                                 EditorSceneState& sceneState)
        {
            const std::string assetKey = MakeAssetKey(relativePath);
            const std::string displayName = ResolveVisibleAssetName(relativePath);
            const bool isTextureAsset = IsTextureAssetPath(relativePath);
            std::filesystem::path absolutePath;
            std::error_code ec;
            bool isDirectory = false;
            if (services.ProjectService && services.ProjectService->get().HasActiveProject())
            {
                absolutePath = services.ProjectService->get().GetActiveProject().Paths.AssetsDirectory / relativePath;
                isDirectory = std::filesystem::is_directory(absolutePath, ec);
            }

            DrawPanelHeader("Inspector", "Selected asset details");

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
            if (ImGui::BeginChild("##InspectorAssetCard", ImVec2(0.0f, isTextureAsset ? 132.0f : 118.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "%s", isTextureAsset ? "Texture Asset" : (isDirectory ? "Folder" : "Asset"));
                ImGui::TextUnformatted("Name");
                ImGui::TextUnformatted(displayName.c_str());
                ImGui::TextDisabled("%s", assetKey.c_str());
                if (!absolutePath.empty())
                    ImGui::TextDisabled("%s", absolutePath.generic_string().c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            if (!isTextureAsset)
            {
                ImGui::TextDisabled("No asset inspector is available for this selection yet.");
                return;
            }

            Life::Assets::TextureSpecification specification = LoadTextureSpecification(assetKey, services);
            bool changed = false;

            ImGui::SeparatorText("Import Settings");
            ImGui::TextUnformatted("Min Filter");
            changed |= DrawTextureFilterCombo("##TextureMinFilter", specification.MinFilter);
            ImGui::TextUnformatted("Mag Filter");
            changed |= DrawTextureFilterCombo("##TextureMagFilter", specification.MagFilter);
            ImGui::TextUnformatted("Wrap U");
            changed |= DrawTextureWrapCombo("##TextureWrapU", specification.WrapU);
            ImGui::TextUnformatted("Wrap V");
            changed |= DrawTextureWrapCombo("##TextureWrapV", specification.WrapV);
            changed |= ImGui::Checkbox("Generate Mipmaps", &specification.GenerateMipmaps);
            changed |= ImGui::Checkbox("Flip Vertically On Load", &specification.FlipVerticallyOnLoad);

            if (changed)
                PersistTextureSpecification(relativePath, specification, services, sceneState);

            if (services.AssetManager)
            {
                if (auto textureAsset = services.AssetManager->get().GetCachedByKey<Life::Assets::TextureAsset>(assetKey))
                {
                    if (const Life::TextureResource* textureResource = textureAsset->TryGetTextureResource())
                    {
                        ImGui::SeparatorText("Runtime");
                        ImGui::TextDisabled("%u x %u", textureResource->GetWidth(), textureResource->GetHeight());
                    }
                }
            }
        }
        #endif

        bool HasAddableComponents(const Life::Entity& entity)
        {
            for (const EditorComponentDescriptor& descriptor : EditorComponentRegistry::Get().GetDescriptors())
            {
                if (descriptor.CanAddComponent && descriptor.CanAddComponent(entity))
                    return true;
            }

            return false;
        }

        void RenderSelectedEntityInspector(Life::Scene& scene,
                                           Life::SceneService& sceneService,
                                           Life::Entity selectedEntity,
                                           const EditorServices& services,
                                           EditorSceneState& sceneState)
        {
            #if __has_include(<imgui.h>)
                        bool changed = false;
                        const bool hasAddableComponents = HasAddableComponents(selectedEntity);

                        DrawPanelHeader("Inspector", "Selected entity details");

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
                        if (ImGui::BeginChild("##InspectorEntityCard", ImVec2(0.0f, 112.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                        {
                            bool isEnabled = selectedEntity.IsEnabled();
                            if (ImGui::BeginTable("##InspectorEntityCardHeader", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
                            {
                                ImGui::TableSetupColumn("##EntityTitle", ImGuiTableColumnFlags_WidthStretch);
                                ImGui::TableSetupColumn("##EntityEnabled", ImGuiTableColumnFlags_WidthFixed, 104.0f);
                                ImGui::TableNextRow();

                                ImGui::TableSetColumnIndex(0);
                                ImGui::AlignTextToFramePadding();
                                ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Entity");

                                ImGui::TableSetColumnIndex(1);
                                const float checkboxWidth = ImGui::CalcTextSize("Enabled").x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().ItemSpacing.x;
                                const float checkboxOffset = std::max(0.0f, ImGui::GetContentRegionAvail().x - checkboxWidth);
                                if (checkboxOffset > 0.0f)
                                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + checkboxOffset);
                                if (ImGui::Checkbox("Enabled", &isEnabled))
                                {
                                    selectedEntity.SetEnabled(isEnabled);
                                    changed = true;
                                }

                                ImGui::EndTable();
                            }

                            ImGui::TextUnformatted("Name");
                            std::string entityName = selectedEntity.GetTag();
                            ImGui::SetNextItemWidth(-1.0f);
                            if (InputTextString("##InspectorEntityName", entityName))
                            {
                                selectedEntity.SetTag(std::move(entityName));
                                changed = true;
                            }
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleVar();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f, 0.20f, 0.22f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.25f, 0.28f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.36f, 0.16f, 0.18f, 1.0f));
                        if (ImGui::Button("Delete Entity", ImVec2(-1.0f, 0.0f)))
                        {
                            const std::string deletedId = selectedEntity.GetId();
                            sceneState.ClearSelection();
                            changed |= scene.DestroyEntity(selectedEntity);
                            if (changed)
                                sceneState.SetStatusMessage("Deleted entity '" + deletedId + "'.", false);
                        }
                        ImGui::PopStyleColor(3);

                        ImGui::SeparatorText("Components");

                        const ImGuiStyle& style = ImGui::GetStyle();
                        const float footerHeight = hasAddableComponents
                            ? ImGui::GetFrameHeightWithSpacing() * 2.0f + style.ItemSpacing.y + style.WindowPadding.y
                            : ImGui::GetTextLineHeightWithSpacing() * 2.0f + style.ItemSpacing.y + style.WindowPadding.y;

                        if (ImGui::BeginChild("InspectorComponents", ImVec2(0.0f, -footerHeight), false))
                        {
                            for (const EditorComponentDescriptor& descriptor : EditorComponentRegistry::Get().GetDescriptors())
                            {
                                if (!descriptor.HasComponent || !descriptor.HasComponent(selectedEntity))
                                    continue;
                                if (descriptor.Id == "tag")
                                    continue;

                                ImGui::PushID(descriptor.Id.c_str());
                                const std::string headerLabel = descriptor.DisplayName + "##" + descriptor.Id;
                                const bool open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                                if (!open)
                                {
                                    ImGui::PopID();
                                    continue;
                                }

                                if (descriptor.Removable)
                                {
                                    if (ImGui::Button("Remove"))
                                    {
                                        changed |= descriptor.RemoveComponent(selectedEntity);
                                        ImGui::PopID();
                                        continue;
                                    }

                                    ImGui::Spacing();
                                }

                                if (descriptor.DrawInspector)
                                    changed |= descriptor.DrawInspector(selectedEntity, services);

                                ImGui::Separator();
                                ImGui::PopID();
                            }
                        }
                        ImGui::EndChild();

                        ImGui::SeparatorText("Add Component");
                        if (hasAddableComponents)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.33f, 0.54f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.41f, 0.64f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.29f, 0.48f, 1.0f));
                            if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
                                ImGui::OpenPopup("AddComponentPopup");
                            ImGui::PopStyleColor(3);

                            if (ImGui::BeginPopup("AddComponentPopup"))
                            {
                                for (const EditorComponentDescriptor& descriptor : EditorComponentRegistry::Get().GetDescriptors())
                                {
                                    if (!descriptor.CanAddComponent || !descriptor.CanAddComponent(selectedEntity))
                                        continue;

                                    if (ImGui::Selectable(descriptor.DisplayName.c_str()))
                                    {
                                        descriptor.AddComponent(selectedEntity);
                                        changed = true;
                                    }
                                }
                                ImGui::EndPopup();
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("No additional components available.");
                        }

                        if (changed && sceneState.ExecutionMode == EditorSceneExecutionMode::Edit)
                            sceneService.MarkActiveSceneDirty();
            #else
                        (void)scene;
                        (void)sceneService;
                        (void)selectedEntity;
                        (void)services;
                        (void)sceneState;
            #endif
        }
    }

    void InspectorPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState) const
    {
        #if __has_include(<imgui.h>)
                if (!isOpen)
                    return;

                if (ImGui::Begin("Inspector", &isOpen))
                {
                    Life::SceneService* sceneService = services.SceneService ? &services.SceneService->get() : nullptr;
                    Life::Scene* effectiveScene = nullptr;
                    Life::Entity selectedEntity;
                    if (sceneService && sceneService->HasActiveScene())
                    {
                        effectiveScene = sceneState.GetEffectiveScene(*sceneService);
                        if (effectiveScene != nullptr)
                            selectedEntity = sceneState.GetSelectedEntity(*effectiveScene);
                        if (!selectedEntity.IsValid() && !sceneState.SelectedEntityId.empty())
                            sceneState.SelectedEntityId.clear();
                    }

                    if (selectedEntity.IsValid() && sceneService != nullptr && effectiveScene != nullptr)
                    {
                        RenderSelectedEntityInspector(*effectiveScene, *sceneService, selectedEntity, services, sceneState);
                    }
                    else
                    {
                        const std::filesystem::path selectedProjectAsset = sceneState.GetSelectedProjectAssetRelativePath();
                        if (!selectedProjectAsset.empty())
                        {
                            RenderSelectedProjectAssetInspector(selectedProjectAsset, services, sceneState);
                        }
                        else
                        {
                            DrawPanelHeader("Inspector", "Selected details");
                            ImGui::TextUnformatted(sceneService && sceneService->HasActiveScene()
                                ? "Select an entity or project asset to inspect it."
                                : "Select a project asset to inspect it.");
                        }
                    }
                }
                ImGui::End();
        #else
                (void)isOpen;
                (void)services;
                (void)sceneState;
        #endif
    }
}
