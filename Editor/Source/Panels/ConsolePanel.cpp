#include "Editor/Panels/ConsolePanel.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    void ConsolePanel::Render(bool& isOpen) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Console", &isOpen))
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Console");
            ImGui::SameLine();
            ImGui::TextDisabled("Runtime and editor output");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Editor viewport rendering is active.");
            ImGui::TextUnformatted("ImGui is now hosted by the dedicated Editor app.");
        }
        ImGui::End();
#else
        (void)isOpen;
#endif
    }
}
