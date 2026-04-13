#pragma once

#include <string>

namespace EditorApp
{
    struct EditorPanelVisibility
    {
        bool ShowProjectAssets = true;
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
        bool RequestNewScene = false;
        bool RequestOpenScene = false;
        bool RequestSaveScene = false;
        bool RequestSaveSceneAs = false;
        bool RequestCloseScene = false;
    };

    class EditorShell
    {
    public:
        struct FrameContext
        {
            const char* ActiveProjectName = nullptr;
            const char* ActiveSceneName = nullptr;
            bool HasActiveScene = false;
            bool IsSceneDirty = false;
        };

        void ResetLayout() noexcept;
        void Begin(EditorPanelVisibility& visibility, EditorShellActions& actions, const FrameContext& context);
        void End();

    private:
        void BuildDefaultLayout();
        void RenderMenuBar(EditorPanelVisibility& visibility, EditorShellActions& actions, const FrameContext& context);

        bool m_LayoutInitialized = false;
    };
}
