#pragma once

namespace EditorApp
{
    struct EditorPanelVisibility
    {
        bool ShowHierarchy = true;
        bool ShowInspector = true;
        bool ShowConsole = true;
        bool ShowStats = true;
        bool ShowScene = true;
    };

    class EditorShell
    {
    public:
        void ResetLayout() noexcept;
        void Begin(EditorPanelVisibility& visibility);
        void End();

    private:
        void BuildDefaultLayout();
        void RenderMenuBar(EditorPanelVisibility& visibility);

        bool m_LayoutInitialized = false;
    };
}
