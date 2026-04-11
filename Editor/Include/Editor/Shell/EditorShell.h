#pragma once

namespace EditorApp
{
    struct EditorPanelVisibility
    {
        bool ShowHierarchy = true;
        bool ShowInspector = true;
        bool ShowConsole = true;
        bool ShowRendererStress = true;
        bool ShowStats = true;
        bool ShowScene = true;
        bool ShowFpsOverlay = false;
    };

    struct EditorShellActions
    {
        bool RequestCloseProject = false;
    };

    class EditorShell
    {
    public:
        void ResetLayout() noexcept;
        void Begin(EditorPanelVisibility& visibility, EditorShellActions& actions, const char* activeProjectName);
        void End();

    private:
        void BuildDefaultLayout();
        void RenderMenuBar(EditorPanelVisibility& visibility, EditorShellActions& actions, const char* activeProjectName);

        bool m_LayoutInitialized = false;
    };
}
