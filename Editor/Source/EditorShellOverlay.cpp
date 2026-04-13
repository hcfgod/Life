#include "Editor/EditorShellOverlay.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
#if __has_include(<imgui.h>)
        bool InputTextString(const char* label, std::string& value)
        {
            std::array<char, 1024> buffer{};
            const std::size_t copyLength = std::min(value.size(), buffer.size() - 1);
            std::memcpy(buffer.data(), value.data(), copyLength);
            buffer[copyLength] = '\0';

            if (!ImGui::InputText(label, buffer.data(), buffer.size()))
                return false;

            value = buffer.data();
            return true;
        }
#endif

        std::string SanitizeSceneStem(std::string value)
        {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char character)
            {
                return std::isspace(character) == 0;
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char character)
            {
                return std::isspace(character) == 0;
            }).base(), value.end());

            if (value.empty())
                return "Scene";

            constexpr std::array<char, 9> invalidCharacters{ '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
            for (char& character : value)
            {
                if (std::find(invalidCharacters.begin(), invalidCharacters.end(), character) != invalidCharacters.end())
                    character = '_';
            }

            return value;
        }

        std::string PathToUiString(const std::filesystem::path& path)
        {
            std::filesystem::path preferred = path;
            preferred.make_preferred();
            return preferred.string();
        }

        void TryUpdateProjectStartupScene(Life::Assets::ProjectService& projectService, const Life::SceneService& sceneService)
        {
            if (!projectService.HasActiveProject() || !sceneService.HasActiveSceneSourcePath())
                return;

            Life::Assets::Project& project = projectService.GetActiveProject();
            const std::filesystem::path scenePath = sceneService.GetActiveSceneSourcePath().lexically_normal();
            const std::filesystem::path relativePath = scenePath.lexically_relative(project.Paths.RootDirectory);
            if (relativePath.empty())
                return;

            const std::string relativeString = relativePath.generic_string();
            if (relativeString.rfind("..", 0) == 0)
                return;

            project.Descriptor.Startup.Scene = relativeString;
            (void)projectService.SaveProject();
        }
    }

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
        {
            if (m_Services.SceneService)
                m_Services.SceneService->get().CloseScene();
            m_SceneState.ClearSelection();
            m_SceneState.ClearStatus();
            m_ProjectHub.RefreshRecentProjects();
        }
        else if (mode == Mode::Workspace)
        {
            if (m_Services.ProjectService && m_Services.SceneService)
            {
                Life::Assets::ProjectService& projectService = m_Services.ProjectService->get();
                Life::SceneService& sceneService = m_Services.SceneService->get();
                if (projectService.HasActiveProject() && !sceneService.HasActiveScene())
                {
                    const Life::Assets::Project& project = projectService.GetActiveProject();
                    if (!project.Descriptor.Startup.Scene.empty())
                    {
                        sceneService.OpenScene(project.Descriptor.Startup.Scene);
                        SetSceneStatus("Opened startup scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                    }
                    else
                    {
                        sceneService.CreateScene(project.Descriptor.Name.empty() ? "EditorScene" : project.Descriptor.Name);
                        SetSceneStatus("Created editor scene document.", false);
                    }
                }

                m_SceneState.ClearSelection();
            }
        }
    }

    void EditorShellOverlay::HandleShellActions(const EditorShellActions& actions)
    {
        if (!m_Services.SceneService)
            return;

        Life::SceneService& sceneService = m_Services.SceneService->get();

        if (actions.RequestNewScene)
        {
            m_NewSceneName = "Untitled";
            m_NewScenePath = BuildDefaultScenePath(m_NewSceneName);
            m_OpenNewScenePopup = true;
        }

        if (actions.RequestOpenScene)
        {
            m_OpenScenePath = sceneService.HasActiveSceneSourcePath()
                ? PathToUiString(sceneService.GetActiveSceneSourcePath())
                : BuildDefaultScenePath("Scene");
            m_OpenOpenScenePopup = true;
        }

        if (actions.RequestSaveScene)
        {
            if (!sceneService.HasActiveScene())
            {
                SetSceneStatus("No active scene is available to save.", true);
            }
            else if (sceneService.HasActiveSceneSourcePath())
            {
                const auto saveResult = sceneService.SaveActiveScene();
                if (saveResult.IsFailure())
                {
                    SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
                }
                else
                {
                    if (m_Services.ProjectService)
                        TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                    SetSceneStatus("Saved scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                }
            }
            else
            {
                m_SaveScenePath = BuildDefaultScenePath(sceneService.GetActiveScene().GetName());
                m_OpenSaveSceneAsPopup = true;
            }
        }

        if (actions.RequestSaveSceneAs)
        {
            if (!sceneService.HasActiveScene())
            {
                SetSceneStatus("No active scene is available to save.", true);
            }
            else
            {
                m_SaveScenePath = sceneService.HasActiveSceneSourcePath()
                    ? PathToUiString(sceneService.GetActiveSceneSourcePath())
                    : BuildDefaultScenePath(sceneService.GetActiveScene().GetName());
                m_OpenSaveSceneAsPopup = true;
            }
        }

        if (actions.RequestCloseScene)
        {
            if (sceneService.CloseScene())
            {
                m_SceneState.ClearSelection();
                SetSceneStatus("Scene closed.", false);
            }
        }
    }

    void EditorShellOverlay::RenderSceneDialogs()
    {
#if __has_include(<imgui.h>)
        if (!m_Services.SceneService)
            return;

        Life::SceneService& sceneService = m_Services.SceneService->get();

        if (m_OpenNewScenePopup)
        {
            ImGui::OpenPopup("New Scene");
            m_OpenNewScenePopup = false;
        }
        if (m_OpenOpenScenePopup)
        {
            ImGui::OpenPopup("Open Scene");
            m_OpenOpenScenePopup = false;
        }
        if (m_OpenSaveSceneAsPopup)
        {
            ImGui::OpenPopup("Save Scene As");
            m_OpenSaveSceneAsPopup = false;
        }

        if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Name", m_NewSceneName);
            InputTextString("Initial Path", m_NewScenePath);

            if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
            {
                Life::Scene& scene = sceneService.CreateScene(m_NewSceneName.empty() ? "Untitled" : m_NewSceneName);
                m_SceneState.ClearSelection();
                if (!m_NewScenePath.empty())
                {
                    const auto saveResult = sceneService.SaveActiveSceneAs(m_NewScenePath);
                    if (saveResult.IsFailure())
                    {
                        sceneService.MarkActiveSceneDirty();
                        SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
                    }
                    else
                    {
                        if (m_Services.ProjectService)
                            TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                        SetSceneStatus("Created scene '" + scene.GetName() + "'.", false);
                    }
                }
                else
                {
                    sceneService.MarkActiveSceneDirty();
                    SetSceneStatus("Created unsaved scene '" + scene.GetName() + "'.", false);
                }

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Open Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Path", m_OpenScenePath);

            if (ImGui::Button("Open", ImVec2(120.0f, 0.0f)))
            {
                const auto loadResult = sceneService.LoadScene(m_OpenScenePath);
                if (loadResult.IsFailure())
                {
                    SetSceneStatus(loadResult.GetError().GetErrorMessage(), true);
                }
                else
                {
                    m_SceneState.ClearSelection();
                    if (m_Services.ProjectService)
                        TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                    SetSceneStatus("Opened scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Path", m_SaveScenePath);

            if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
            {
                const auto saveResult = sceneService.SaveActiveSceneAs(m_SaveScenePath);
                if (saveResult.IsFailure())
                {
                    SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
                }
                else
                {
                    if (m_Services.ProjectService)
                        TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                    SetSceneStatus("Saved scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
#endif
    }

    std::string EditorShellOverlay::BuildDefaultScenePath(const std::string& sceneName) const
    {
        const std::string fileName = SanitizeSceneStem(sceneName) + ".scene";
        if (m_Services.ProjectService && m_Services.ProjectService->get().HasActiveProject())
        {
            const Life::Assets::Project& project = m_Services.ProjectService->get().GetActiveProject();
            return PathToUiString(project.Paths.AssetsDirectory / "Scenes" / fileName);
        }

        return fileName;
    }

    void EditorShellOverlay::SetSceneStatus(std::string message, bool isError)
    {
        m_SceneState.SetStatusMessage(message, isError);
        if (isError)
            LOG_WARN("{}", m_SceneState.StatusMessage);
        else
            LOG_INFO("{}", m_SceneState.StatusMessage);
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
            SetMode(Mode::Workspace);
        else
            SetMode(Mode::ProjectHub);

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
        EditorShell::FrameContext frameContext{};
        frameContext.ActiveProjectName = projectService.HasActiveProject()
            ? projectService.GetActiveProject().Descriptor.Name.c_str()
            : nullptr;
        if (m_Services.SceneService && m_Services.SceneService->get().HasActiveScene())
        {
            frameContext.ActiveSceneName = m_Services.SceneService->get().GetActiveScene().GetName().c_str();
            frameContext.HasActiveScene = true;
            frameContext.IsSceneDirty = m_Services.SceneService->get().IsActiveSceneDirty();
        }

        m_Shell.Begin(m_PanelVisibility, actions, frameContext);
        m_ProjectAssetsPanel.Render(m_PanelVisibility.ShowProjectAssets, m_Services, m_SceneState);
        m_HierarchyPanel.Render(m_PanelVisibility.ShowHierarchy, m_Services, m_SceneState);
        m_InspectorPanel.Render(m_PanelVisibility.ShowInspector, m_Services, m_SceneState);
        m_ConsolePanel.Render(m_PanelVisibility.ShowConsole);
        m_SceneViewportPanel.RenderStressPanel(m_PanelVisibility.ShowRendererStress);
        m_StatsPanel.Render(m_PanelVisibility.ShowStats, m_Services, m_SceneViewportPanel.GetState());
        m_SceneViewportPanel.Render(m_PanelVisibility.ShowScene, m_Services, m_SceneState, m_CameraTool);
        m_FpsOverlayPanel.Render(m_PanelVisibility.ShowFpsOverlay);
        RenderSceneDialogs();
        m_Shell.End();

        HandleShellActions(actions);

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
        dispatcher.Dispatch<Life::WindowFileDroppedEvent>([this](Life::WindowFileDroppedEvent& dropEvent)
        {
            if (m_Mode != Mode::Workspace ||
                !m_PanelVisibility.ShowProjectAssets ||
                !m_Services.ProjectService ||
                !m_Services.ProjectService->get().HasActiveProject())
                return false;

            m_ProjectAssetsPanel.QueueExternalFileDrop(dropEvent.GetPath(), dropEvent.GetX(), dropEvent.GetY());
            return false;
        });
    }
}
