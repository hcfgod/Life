#include "Editor/Panels/HierarchyPanel.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include <string>

namespace EditorApp
{
    void HierarchyPanel::Render(bool& isOpen, const Life::Application& application) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Hierarchy", &isOpen))
        {
            const Life::LayerStack& layerStack = application.GetLayerStack();
            const std::size_t regularLayerCount = layerStack.GetRegularLayerCount();
            std::size_t index = 0;
            for (const Life::LayerRef& layer : layerStack)
            {
                if (!layer)
                {
                    ++index;
                    continue;
                }

                ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (layer->IsEnabled())
                    nodeFlags |= ImGuiTreeNodeFlags_Bullet;

                const bool isOverlay = index >= regularLayerCount;
                std::string label = layer->GetDebugName();
                label += isOverlay ? " (Overlay)" : " (Layer)";
                ImGui::TreeNodeEx(label.c_str(), nodeFlags);
                ++index;
            }
        }
        ImGui::End();
#else
        (void)isOpen;
        (void)application;
#endif
    }
}
