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
        void DrawPanelHeader(const char* title, const char* subtitle)
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "%s", title);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", subtitle);
            ImGui::Separator();
        }

        DropMode DetermineDropMode(const ImVec2& rectMin, const ImVec2& rectMax)
        {
            const float itemHeight = rectMax.y - rectMin.y;
            const float mouseY = ImGui::GetIO().MousePos.y;
            if (itemHeight > 0.0f)
            {
                if (mouseY < rectMin.y + itemHeight * 0.25f)
                    return DropMode::Before;
                if (mouseY > rectMax.y - itemHeight * 0.25f)
                    return DropMode::After;
            }

            return DropMode::Child;
        }

        bool CanApplyDrop(const Life::Entity& dragged, const Life::Entity& target)
        {
            return dragged.IsValid() && target.IsValid() && dragged != target && !target.IsDescendantOf(dragged);
        }

        void DrawDropPreview(const ImVec2& rectMin, const ImVec2& rectMax, DropMode mode, bool valid)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 lineColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.95f) : ImVec4(0.90f, 0.26f, 0.26f, 0.95f));
            const ImU32 fillColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.14f) : ImVec4(0.90f, 0.26f, 0.26f, 0.12f));
            const float thickness = 2.0f;

            switch (mode)
            {
                case DropMode::Child:
                    drawList->AddRectFilled(rectMin, rectMax, fillColor, 4.0f);
                    drawList->AddRect(rectMin, rectMax, lineColor, 4.0f, 0, thickness);
                    break;

                case DropMode::Before:
                    drawList->AddLine(ImVec2(rectMin.x, rectMin.y + 1.0f), ImVec2(rectMax.x, rectMin.y + 1.0f), lineColor, thickness);
                    break;

                case DropMode::After:
                    drawList->AddLine(ImVec2(rectMin.x, rectMax.y - 1.0f), ImVec2(rectMax.x, rectMax.y - 1.0f), lineColor, thickness);
                    break;
            }
        }

        void DrawRootDropPreview(const ImVec2& rectMin, const ImVec2& rectMax, bool valid)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 lineColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.95f) : ImVec4(0.90f, 0.26f, 0.26f, 0.95f));
            const ImU32 fillColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.14f) : ImVec4(0.90f, 0.26f, 0.26f, 0.12f));
            drawList->AddRectFilled(rectMin, rectMax, fillColor, 4.0f);
            drawList->AddRect(rectMin, rectMax, lineColor, 4.0f, 0, 2.0f);
        }

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
            if (!CanApplyDrop(dragged, target))
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
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                const Life::Entity dragged = ResolvePayloadEntity(payload, scene);
                const ImVec2 rectMin = ImGui::GetItemRectMin();
                const ImVec2 rectMax = ImGui::GetItemRectMax();
                const DropMode mode = DetermineDropMode(rectMin, rectMax);
                const bool valid = CanApplyDrop(dragged, target);

                DrawDropPreview(rectMin, rectMax, mode, valid);

                if (payload->Delivery && valid)
                    changed = ApplyDrop(scene, dragged, target, mode);
            }

            ImGui::EndDragDropTarget();
            return changed;
        }

        bool RenderRootDropTarget(Life::Scene& scene)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Scene Root");
            const ImVec2 targetSize(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.35f);
            ImGui::Selectable("Drop here to reparent to root", false, ImGuiSelectableFlags_None, targetSize);

            bool changed = false;
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    Life::Entity dragged = ResolvePayloadEntity(payload, scene);
                    const bool valid = dragged.IsValid() && dragged.HasParent();
                    DrawRootDropPreview(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), valid);

                    if (payload->Delivery && valid)
                    {
                        dragged.RemoveParent();
                        changed = true;
                    }
                }

                ImGui::EndDragDropTarget();
            }

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
            DrawPanelHeader("Hierarchy", "Scene structure and parenting");

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

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.33f, 0.54f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.41f, 0.64f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.29f, 0.48f, 1.0f));
                if (ImGui::Button("Create Entity", ImVec2(-1.0f, 0.0f)))
                {
                    const Life::Entity entity = scene.CreateEntity("Entity");
                    sceneState.SelectEntity(entity);
                    changed = true;
                }
                ImGui::PopStyleColor(3);

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

                ImGui::SeparatorText("Entities");

                const auto roots = scene.GetRootEntities();
                if (roots.empty())
                    ImGui::TextDisabled("No entities in the active scene.");
                for (const Life::Entity root : roots)
                    changed |= RenderEntityNode(scene, root, sceneState, pendingDelete);

                changed |= RenderRootDropTarget(scene);

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
