#include "Editor/EditorShellOverlay.h"

namespace EditorApp
{
    EditorShellOverlay::EditorShellOverlay()
        : Life::Layer("EditorShellOverlay")
    {
    }

    void EditorShellOverlay::SetMode(Mode mode)
    {
        if (m_Mode == mode)
            return;

        m_Mode = mode;
        m_Shell.ResetLayout();

        if (mode == Mode::ProjectHub)
            m_ProjectHub.RefreshRecentProjects();
    }

    void EditorShellOverlay::OnAttach()
    {
        m_Services = EditorServices::Acquire(GetApplication());
        m_Shell.ResetLayout();
        m_ProjectHub.Attach();

        if (m_Services.CameraManager)
            m_CameraTool.Ensure(m_Services.CameraManager->get(), 16.0f / 9.0f);

        m_SceneViewportPanel.Attach(m_Services);

        if (m_Services.ProjectService && m_Services.ProjectService->get().HasActiveProject())
            m_Mode = Mode::Workspace;
        else
            m_Mode = Mode::ProjectHub;

        LOG_INFO("Editor shell overlay attached.");
    }

    void EditorShellOverlay::OnDetach()
    {
        m_ProjectHub.Detach();
        m_SceneViewportPanel.Detach();

        if (m_Services.CameraManager)
            m_CameraTool.Release(m_Services.CameraManager->get());

        m_Services.Reset();
        LOG_INFO("Editor shell overlay detached.");
    }

    void EditorShellOverlay::OnUpdate(float timestep)
    {
        if (m_Mode == Mode::Workspace)
        {
            m_SceneViewportPanel.Update(m_Services, timestep);
            m_FpsOverlayPanel.Update(timestep);
        }

        if (m_Services.InputSystem && m_Services.Application && m_Services.InputSystem->get().WasActionStartedThisFrame("Editor", "Quit"))
            m_Services.Application->get().RequestShutdown();
    }

    void EditorShellOverlay::OnRender()
    {
        if (!m_Services.Application || !m_Services.HasImGui() || !m_Services.ProjectService)
            return;

        Life::Assets::ProjectService& projectService = m_Services.ProjectService->get();
        if (m_Mode == Mode::ProjectHub)
        {
            if (m_ProjectHub.Render(projectService))
                SetMode(Mode::Workspace);
            return;
        }

        EditorShellActions actions{};
        const char* activeProjectName = projectService.HasActiveProject()
            ? projectService.GetActiveProject().Descriptor.Name.c_str()
            : nullptr;

        m_Shell.Begin(m_PanelVisibility, actions, activeProjectName);
        m_HierarchyPanel.Render(m_PanelVisibility.ShowHierarchy, m_Services.Application->get());
        m_InspectorPanel.Render(m_PanelVisibility.ShowInspector, m_Services, m_CameraTool);
        m_ConsolePanel.Render(m_PanelVisibility.ShowConsole);
        m_SceneViewportPanel.RenderStressPanel(m_PanelVisibility.ShowRendererStress);
        m_StatsPanel.Render(m_PanelVisibility.ShowStats, m_Services, m_SceneViewportPanel.GetState());
        m_SceneViewportPanel.Render(m_PanelVisibility.ShowScene, m_Services, m_CameraTool);
        m_FpsOverlayPanel.Render(m_PanelVisibility.ShowFpsOverlay);
        m_Shell.End();

        if (actions.RequestCloseProject)
        {
            const auto closeResult = projectService.CloseProject();
            if (closeResult.IsFailure())
                m_ProjectHub.SetStatusMessage(closeResult.GetError().GetErrorMessage(), true);
            else
            {
                m_ProjectHub.SetStatusMessage("Project closed.", false);
                SetMode(Mode::ProjectHub);
            }
        }
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
