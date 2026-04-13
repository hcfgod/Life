#pragma once

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

    inline bool operator==(const EditorPanelVisibility& lhs, const EditorPanelVisibility& rhs) noexcept
    {
        return lhs.ShowProjectAssets == rhs.ShowProjectAssets
            && lhs.ShowHierarchy == rhs.ShowHierarchy
            && lhs.ShowInspector == rhs.ShowInspector
            && lhs.ShowConsole == rhs.ShowConsole
            && lhs.ShowRendererStress == rhs.ShowRendererStress
            && lhs.ShowStats == rhs.ShowStats
            && lhs.ShowScene == rhs.ShowScene
            && lhs.ShowFpsOverlay == rhs.ShowFpsOverlay;
    }

    inline bool operator!=(const EditorPanelVisibility& lhs, const EditorPanelVisibility& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    struct EditorShellActions
    {
        bool RequestCloseProject = false;
        bool RequestNewScene = false;
        bool RequestOpenScene = false;
        bool RequestSaveScene = false;
        bool RequestSaveSceneAs = false;
        bool RequestCloseScene = false;
    };
}
