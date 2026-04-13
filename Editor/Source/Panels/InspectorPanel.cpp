#include "Editor/Panels/InspectorPanel.h"

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorComponentRegistry.h"

#include <string>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
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

                    ImGui::Text("Entity: %s", selectedEntity.GetTag().c_str());
                    ImGui::TextDisabled("ID: %s", selectedEntity.GetId().c_str());
                    ImGui::Spacing();

                    if (ImGui::Button("Delete Entity"))
                    {
                        const std::string deletedId = selectedEntity.GetId();
                        sceneState.ClearSelection();
                        changed |= sceneService.GetActiveScene().DestroyEntity(selectedEntity);
                        if (changed)
                            sceneState.SetStatusMessage("Deleted entity '" + deletedId + "'.", false);
                    }

                    ImGui::Separator();

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

                    ImGui::Separator();
                    ImGui::TextUnformatted("Add Component");
                    if (hasAddableComponents)
                    {
                        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
                            ImGui::OpenPopup("AddComponentPopup");

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
