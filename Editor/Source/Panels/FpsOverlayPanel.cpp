#include "Editor/Panels/FpsOverlayPanel.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include <algorithm>

namespace EditorApp
{
    void FpsOverlayPanel::Update(float timestep) noexcept
    {
        const float sanitizedTimestep = std::max(timestep, 0.0001f);
        constexpr float smoothingFactor = 0.10f;

        m_LastFrameSeconds = sanitizedTimestep;
        m_SmoothedFrameSeconds += (sanitizedTimestep - m_SmoothedFrameSeconds) * smoothingFactor;
    }

    void FpsOverlayPanel::Render(bool& isOpen) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 padding(16.0f, 16.0f);
        const ImVec2 position(viewport->WorkPos.x + viewport->WorkSize.x - padding.x, viewport->WorkPos.y + padding.y);
        ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.72f);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoDocking |
                                             ImGuiWindowFlags_NoSavedSettings |
                                             ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("FPS Overlay", &isOpen, windowFlags))
        {
            const float smoothedMilliseconds = m_SmoothedFrameSeconds * 1000.0f;
            const float smoothedFps = smoothedMilliseconds > 0.0f ? 1000.0f / smoothedMilliseconds : 0.0f;
            const float lastFrameMilliseconds = m_LastFrameSeconds * 1000.0f;

            ImGui::Text("FPS %.1f", smoothedFps);
            ImGui::Text("Frame %.2f ms", smoothedMilliseconds);
            ImGui::Separator();
            ImGui::Text("Last %.2f ms", lastFrameMilliseconds);
        }
        ImGui::End();
#else
        (void)isOpen;
#endif
    }
}
