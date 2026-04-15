#pragma once

#include "Editor/Camera/EditorCameraTool.h"
#include "Editor/EditorServices.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/FpsOverlayPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ProjectAssetsPanel.h"
#include "Editor/Scene/EditorSceneState.h"
#include "Editor/Panels/StatsPanel.h"
#include "Editor/ProjectHub/EditorProjectHub.h"
#include "Editor/Shell/EditorShell.h"
#include "Editor/Viewport/SceneViewportPanel.h"
#include "Engine.h"

namespace EditorApp
{
    class EditorShellOverlay final : public Life::Layer
    {
    public:
        EditorShellOverlay();

    protected:
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(float timestep) override;
        void OnRender() override;
        void OnEvent(Life::Event& event) override;

    private:
        enum class Mode
        {
            ProjectHub,
            Workspace
        };

        void SetMode(Mode mode);
        void HandleShellActions(const EditorShellActions& actions);
        void RenderSceneDialogs();
        std::string BuildDefaultScenePath(const std::string& sceneName) const;
        void SetSceneStatus(std::string message, bool isError);
        bool BeginSceneExecution(EditorSceneExecutionMode executionMode);
        void StopSceneExecution();
        void UpdateSceneExecution(float timestep);

        EditorServices m_Services;
        EditorPanelVisibility m_PanelVisibility;
        EditorPanelState m_PanelState;
        EditorShell m_Shell;
        EditorProjectHub m_ProjectHub;
        EditorCameraTool m_CameraTool;
        SceneViewportPanel m_SceneViewportPanel;
        ProjectAssetsPanel m_ProjectAssetsPanel;
        HierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        EditorSceneState m_SceneState;
        ConsolePanel m_ConsolePanel;
        FpsOverlayPanel m_FpsOverlayPanel;
        StatsPanel m_StatsPanel;
        std::string m_NewSceneName = "Untitled";
        std::string m_NewScenePath;
        std::string m_OpenScenePath;
        std::string m_SaveScenePath;
        bool m_OpenNewScenePopup = false;
        bool m_OpenOpenScenePopup = false;
        bool m_OpenSaveSceneAsPopup = false;
        Mode m_Mode = Mode::ProjectHub;
    };
}
