#include "Editor/Panels/InspectorPanel.h"

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorComponentRegistry.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

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
    }

    void InspectorPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Inspector", &isOpen))
        {
            DrawPanelHeader("Inspector", "Selected entity details");

            if (!services.SceneService || !services.SceneService->get().HasActiveScene())
            {
                ImGui::TextUnformatted("No active scene.");
            }
            else
            {
                Life::SceneService& sceneService = services.SceneService->get();
                Life::Entity selectedEntity = sceneState.GetSelectedEntity(sceneService);
                if (!selectedEntity.IsValid())
                {
                    sceneState.ClearSelection();
                    ImGui::TextUnformatted("Select an entity to inspect it.");
                }
                else
                {
                    bool changed = false;
                    const bool hasAddableComponents = HasAddableComponents(selectedEntity);

                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
                    if (ImGui::BeginChild("##InspectorEntityCard", ImVec2(0.0f, 98.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                    {
                        bool isEnabled = selectedEntity.IsEnabled();
                        if (ImGui::BeginTable("##InspectorEntityCardHeader", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
                        {
                            ImGui::TableSetupColumn("##EntityTitle", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableSetupColumn("##EntityEnabled", ImGuiTableColumnFlags_WidthFixed, 104.0f);
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Entity");
                            ImGui::SameLine();
                            ImGui::TextDisabled("Primary selection");

                            ImGui::TableSetColumnIndex(1);
                            ImGui::AlignTextToFramePadding();
                            if (ImGui::Checkbox("Enabled", &isEnabled))
                            {
                                selectedEntity.SetEnabled(isEnabled);
                                changed = true;
                            }

                            ImGui::EndTable();
                        }

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
                        changed |= sceneService.GetActiveScene().DestroyEntity(selectedEntity);
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

                    if (changed)
                        sceneService.MarkActiveSceneDirty();
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
