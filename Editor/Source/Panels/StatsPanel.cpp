#include "Editor/Panels/StatsPanel.h"

#include "Editor/EditorServices.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    void StatsPanel::Render(bool& isOpen, const EditorServices& services, const SceneViewportState& viewportState) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Stats", &isOpen))
        {
            const Life::GraphicsBackend backend = services.ImGuiSystem
                ? services.ImGuiSystem->get().GetBackend()
                : Life::GraphicsBackend::None;

            ImGui::Text("Graphics Backend: %s", backend == Life::GraphicsBackend::Vulkan ? "Vulkan" : backend == Life::GraphicsBackend::D3D12 ? "D3D12" : "None");
            ImGui::Text("Scene Surface Size: %u x %u", viewportState.SurfaceWidth, viewportState.SurfaceHeight);
            ImGui::Text("Scene Surface Ready: %s", viewportState.SurfaceReady ? "true" : "false");
            ImGui::Text("Scene Render Succeeded: %s", viewportState.LastRenderSucceeded ? "true" : "false");

            if (services.ImGuiSystem)
            {
                ImGui::Text("ImGui Keyboard Capture: %s", services.ImGuiSystem->get().WantsKeyboardCapture() ? "true" : "false");
                ImGui::Text("ImGui Mouse Capture: %s", services.ImGuiSystem->get().WantsMouseCapture() ? "true" : "false");
            }

            ImGui::Separator();
            ImGui::Text("Renderer2D Draw Calls: %u", viewportState.RendererStats.DrawCalls);
            ImGui::Text("Renderer2D Quads: %u", viewportState.RendererStats.QuadCount);
        }
        ImGui::End();
#else
        (void)isOpen;
        (void)services;
        (void)viewportState;
#endif
    }
}
