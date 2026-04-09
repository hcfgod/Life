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
            ImGui::TextUnformatted("Editor viewport rendering is active.");
            ImGui::Separator();
            ImGui::TextUnformatted("ImGui is now hosted by the dedicated Editor app.");
        }
        ImGui::End();
#else
        (void)isOpen;
#endif
    }
}
