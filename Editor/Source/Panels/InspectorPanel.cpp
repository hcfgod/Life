#include "Editor/Panels/InspectorPanel.h"

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorComponentRegistry.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
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

                    ImGui::Text("Entity: %s", selectedEntity.GetTag().c_str());
                    ImGui::TextDisabled("ID: %s", selectedEntity.GetId().c_str());

                    if (ImGui::Button("Delete Entity"))
                    {
                        const std::string deletedId = selectedEntity.GetId();
                        sceneState.ClearSelection();
                        changed |= sceneService.GetActiveScene().DestroyEntity(selectedEntity);
                        if (changed)
                            sceneState.SetStatusMessage("Deleted entity '" + deletedId + "'.", false);
                    }

                    ImGui::SameLine();
                    if (ImGui::BeginCombo("Add Component", "Add..."))
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
                        ImGui::EndCombo();
                    }

                    ImGui::Separator();

                    for (const EditorComponentDescriptor& descriptor : EditorComponentRegistry::Get().GetDescriptors())
                    {
                        if (!descriptor.HasComponent || !descriptor.HasComponent(selectedEntity))
                            continue;

                        const bool open = ImGui::CollapsingHeader(descriptor.DisplayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                        if (!open)
                            continue;

                        if (descriptor.Removable)
                        {
                            ImGui::PushID(descriptor.Id.c_str());
                            if (ImGui::Button("Remove"))
                            {
                                changed |= descriptor.RemoveComponent(selectedEntity);
                                ImGui::PopID();
                                continue;
                            }
                            ImGui::PopID();
                        }

                        if (descriptor.DrawInspector)
                            changed |= descriptor.DrawInspector(selectedEntity, services);

                        ImGui::Separator();
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
