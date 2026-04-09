#pragma once

#include "Editor/Camera/EditorCameraTool.h"
#include "Editor/EditorServices.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/StatsPanel.h"
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
        EditorServices m_Services;
        EditorPanelVisibility m_PanelVisibility;
        EditorShell m_Shell;
        EditorCameraTool m_CameraTool;
        SceneViewportPanel m_SceneViewportPanel;
        HierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        ConsolePanel m_ConsolePanel;
        StatsPanel m_StatsPanel;
    };
}
