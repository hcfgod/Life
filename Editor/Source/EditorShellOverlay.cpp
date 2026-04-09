#include "Editor/EditorShellOverlay.h"

namespace EditorApp
{
    EditorShellOverlay::EditorShellOverlay()
        : Life::Layer("EditorShellOverlay")
    {
    }

    void EditorShellOverlay::OnAttach()
    {
        m_Services = EditorServices::Acquire(GetApplication());
        m_Shell.ResetLayout();

        if (m_Services.CameraManager)
            m_CameraTool.Ensure(m_Services.CameraManager->get(), 16.0f / 9.0f);

        m_SceneViewportPanel.Attach(m_Services);

        LOG_INFO("Editor shell overlay attached.");
    }

    void EditorShellOverlay::OnDetach()
    {
        m_SceneViewportPanel.Detach();

        if (m_Services.CameraManager)
            m_CameraTool.Release(m_Services.CameraManager->get());

        m_Services.Reset();
        LOG_INFO("Editor shell overlay detached.");
    }

    void EditorShellOverlay::OnUpdate(float timestep)
    {
        m_SceneViewportPanel.Update(m_Services, timestep);

        if (m_Services.InputSystem && m_Services.Application && m_Services.InputSystem->get().WasActionStartedThisFrame("Editor", "Quit"))
            m_Services.Application->get().RequestShutdown();
    }

    void EditorShellOverlay::OnRender()
    {
        if (!m_Services.Application || !m_Services.HasImGui())
            return;

        m_Shell.Begin(m_PanelVisibility);
        m_HierarchyPanel.Render(m_PanelVisibility.ShowHierarchy, m_Services.Application->get());
        m_InspectorPanel.Render(m_PanelVisibility.ShowInspector, m_Services, m_CameraTool);
        m_ConsolePanel.Render(m_PanelVisibility.ShowConsole);
        m_StatsPanel.Render(m_PanelVisibility.ShowStats, m_Services, m_SceneViewportPanel.GetState());
        m_SceneViewportPanel.Render(m_PanelVisibility.ShowScene, m_Services, m_CameraTool);
        m_Shell.End();
    }

    void EditorShellOverlay::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowResizeEvent>([this](Life::WindowResizeEvent&)
        {
            m_Shell.ResetLayout();
            return false;
        });
    }
}
