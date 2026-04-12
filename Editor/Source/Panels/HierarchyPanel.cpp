#include "Editor/Panels/HierarchyPanel.h"

#include "Editor/EditorServices.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include <array>
#include <cstring>
#include <string>

namespace EditorApp
{
    namespace
    {
        constexpr const char* kEntityPayloadType = "EditorSceneEntity";

        enum class DropMode
        {
            Child,
            Before,
            After
        };

#if __has_include(<imgui.h>)
        bool BeginEntityDragSource(const Life::Entity& entity)
        {
            if (!entity.IsValid() || !ImGui::BeginDragDropSource())
                return false;

            const std::string& entityId = entity.GetId();
            ImGui::SetDragDropPayload(kEntityPayloadType, entityId.c_str(), entityId.size() + 1u);
            ImGui::TextUnformatted(entity.GetTag().c_str());
            return true;
        }

        Life::Entity ResolvePayloadEntity(const ImGuiPayload* payload, Life::Scene& scene)
        {
            if (payload == nullptr || payload->Data == nullptr || payload->DataSize <= 0)
                return {};

            const char* payloadText = static_cast<const char*>(payload->Data);
            return scene.FindEntityById(payloadText);
        }

        bool ApplyDrop(Life::Scene& scene, Life::Entity dragged, const Life::Entity& target, DropMode mode)
        {
            if (!dragged.IsValid() || !target.IsValid() || dragged == target || target.IsDescendantOf(dragged))
                return false;

            switch (mode)
            {
                case DropMode::Child:
                    return dragged.SetParent(target);

                case DropMode::Before:
                {
                    const Life::Entity parent = target.GetParent();
                    if (parent.IsValid())
                    {
                        if (!dragged.SetParent(parent))
                            return false;
                    }
                    else
                    {
                        dragged.RemoveParent();
                    }

                    return scene.SetSiblingIndex(dragged, scene.GetSiblingIndex(target));
                }

                case DropMode::After:
                {
                    const Life::Entity parent = target.GetParent();
                    if (parent.IsValid())
                    {
                        if (!dragged.SetParent(parent))
                            return false;
                    }
                    else
                    {
                        dragged.RemoveParent();
                    }

                    return scene.SetSiblingIndex(dragged, scene.GetSiblingIndex(target) + 1u);
                }
            }

            return false;
        }

        bool AcceptEntityDrop(Life::Scene& scene, const Life::Entity& target)
        {
            if (!ImGui::BeginDragDropTarget())
                return false;

            bool changed = false;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayloadType))
            {
                const Life::Entity dragged = ResolvePayloadEntity(payload, scene);
                const ImVec2 rectMin = ImGui::GetItemRectMin();
                const ImVec2 rectMax = ImGui::GetItemRectMax();
                const float itemHeight = rectMax.y - rectMin.y;
                const float mouseY = ImGui::GetIO().MousePos.y;

                DropMode mode = DropMode::Child;
                if (itemHeight > 0.0f)
                {
                    if (mouseY < rectMin.y + itemHeight * 0.25f)
                        mode = DropMode::Before;
                    else if (mouseY > rectMax.y - itemHeight * 0.25f)
                        mode = DropMode::After;
                }

                changed = ApplyDrop(scene, dragged, target, mode);
            }

            ImGui::EndDragDropTarget();
            return changed;
        }

        bool RenderEntityNode(Life::Scene& scene, const Life::Entity& entity, EditorSceneState& sceneState, Life::Entity& pendingDelete)
        {
            bool changed = false;
            const bool isSelected = sceneState.SelectedEntityId == entity.GetId();
            const auto children = entity.GetChildren();

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (children.empty())
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            if (isSelected)
                nodeFlags |= ImGuiTreeNodeFlags_Selected;

            const bool nodeOpen = ImGui::TreeNodeEx(entity.GetId().c_str(), nodeFlags, "%s", entity.GetTag().c_str());
            if (ImGui::IsItemClicked())
                sceneState.SelectEntity(entity);

            if (BeginEntityDragSource(entity))
                ImGui::EndDragDropSource();

            changed |= AcceptEntityDrop(scene, entity);

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Create Child"))
                {
                    const Life::Entity child = scene.CreateChildEntity(entity, "Entity");
                    sceneState.SelectEntity(child);
                    changed = true;
                }

                if (ImGui::MenuItem("Delete Entity"))
                    pendingDelete = entity;

                ImGui::EndPopup();
            }

            if (nodeOpen)
            {
                for (const Life::Entity child : children)
                    changed |= RenderEntityNode(scene, child, sceneState, pendingDelete);
                ImGui::TreePop();
            }

            return changed;
        }
#endif
    }

    void HierarchyPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Hierarchy", &isOpen))
        {
            if (!sceneState.StatusMessage.empty())
            {
                const ImVec4 color = sceneState.StatusIsError
                    ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                    : ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
                ImGui::TextColored(color, "%s", sceneState.StatusMessage.c_str());
                ImGui::Separator();
            }

            if (!services.SceneService || !services.SceneService->get().HasActiveScene())
            {
                ImGui::TextUnformatted("No active scene.");
            }
            else
            {
                Life::SceneService& sceneService = services.SceneService->get();
                Life::Scene& scene = sceneService.GetActiveScene();
                bool changed = false;
                Life::Entity pendingDelete;

                if (ImGui::Button("Create Entity"))
                {
                    const Life::Entity entity = scene.CreateEntity("Entity");
                    sceneState.SelectEntity(entity);
                    changed = true;
                }

                if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
                {
                    if (ImGui::MenuItem("Create Entity"))
                    {
                        const Life::Entity entity = scene.CreateEntity("Entity");
                        sceneState.SelectEntity(entity);
                        changed = true;
                    }
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                const auto roots = scene.GetRootEntities();
                for (const Life::Entity root : roots)
                    changed |= RenderEntityNode(scene, root, sceneState, pendingDelete);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextUnformatted("Drop here to reparent to root");
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayloadType))
                    {
                        Life::Entity dragged = ResolvePayloadEntity(payload, scene);
                        if (dragged.IsValid())
                        {
                            dragged.RemoveParent();
                            changed = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (pendingDelete.IsValid())
                {
                    if (sceneState.SelectedEntityId == pendingDelete.GetId())
                        sceneState.ClearSelection();
                    changed |= scene.DestroyEntity(pendingDelete);
                }

                if (changed)
                    sceneService.MarkActiveSceneDirty();
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
