#include "Editor/Shell/EditorShell.h"

#include "Assets/Project.h"
#include "Core/Log.h"

#include <algorithm>
#include <array>
#include <cstring>

#if __has_include(<imgui.h>)
#include <imgui.h>
#include <imgui_internal.h>
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

        const char* ResolveExecutionModeLabel(EditorSceneExecutionMode executionMode)
        {
            switch (executionMode)
            {
                case EditorSceneExecutionMode::Play: return "Play";
                case EditorSceneExecutionMode::Simulation: return "Simulation";
                case EditorSceneExecutionMode::Edit:
                default: return "Edit";
            }
        }
#endif
    }

    void EditorShell::ResetLayout() noexcept
    {
        m_LayoutInitialized = false;
        m_NextPersistTime = 0.0;
    }

    void EditorShell::Begin(EditorPanelVisibility& visibility, EditorPanelState& panelState, EditorShellActions& actions, const FrameContext& context)
    {
#if __has_include(<imgui.h>)
        UpdateProjectContext(context);

        ImGuiWindowFlags dockspaceWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(1.0f);

        dockspaceWindowFlags |= ImGuiWindowFlags_NoTitleBar;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoCollapse;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoResize;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoMove;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorRoot", nullptr, dockspaceWindowFlags);
        ImGui::PopStyleVar(3);

        RestoreStartupLayout(visibility, panelState);
        ProcessPendingLayoutCommand(visibility, panelState);
        if (!m_LayoutInitialized && m_UseDefaultLayout)
            BuildDefaultLayout();

        RenderMenuBar(visibility, panelState, actions, context);
        RenderWorkspaceChrome(actions, context);

        const ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        RenderLayoutDialogs(visibility, panelState);
#else
        (void)visibility;
        (void)panelState;
        (void)actions;
        (void)context;
#endif
    }

    void EditorShell::End(const EditorPanelVisibility& visibility, const EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        ImGui::End();
        PersistLayoutSessions(visibility, panelState);
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::BuildDefaultLayout()
    {
#if __has_include(<imgui.h>)
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        if (m_LayoutInitialized)
            return;

        m_LayoutInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        	ImGuiID rootDockId = dockspaceId;
        ImGuiID leftDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Left, 0.19f, nullptr, &rootDockId);
        ImGuiID rightDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Right, 0.26f, nullptr, &rootDockId);
        ImGuiID bottomDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Down, 0.30f, nullptr, &rootDockId);
        ImGuiID projectDockId = ImGui::DockBuilderSplitNode(bottomDockId, ImGuiDir_Left, 0.58f, nullptr, &bottomDockId);

        ImGui::DockBuilderDockWindow("Project Assets", projectDockId);
        ImGui::DockBuilderDockWindow("Hierarchy", leftDockId);
        ImGui::DockBuilderDockWindow("Inspector", rightDockId);
        ImGui::DockBuilderDockWindow("Console", bottomDockId);
        ImGui::DockBuilderDockWindow("Stats", rightDockId);
        ImGui::DockBuilderDockWindow("Scene", rootDockId);
        ImGui::DockBuilderFinish(dockspaceId);
#endif
    }

    void EditorShell::UpdateProjectContext(const FrameContext& context)
    {
        if (m_LayoutManager.GetActiveProject() == context.ActiveProject)
            return;

        m_LayoutManager.SetActiveProject(context.ActiveProject);
        m_LayoutInitialized = false;
        m_StartupLayoutResolved = false;
        m_UseDefaultLayout = true;
        m_ActiveLayout.reset();
        m_LastPersistedProjectSession.reset();
        m_LastPersistedGlobalSession.reset();
        m_NextPersistTime = 0.0;
        m_PendingLayoutCommand = {};
    }

    void EditorShell::RestoreStartupLayout(EditorPanelVisibility& visibility, EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        if (m_StartupLayoutResolved)
            return;

        m_StartupLayoutResolved = true;

        if (m_LayoutManager.HasProjectScope())
        {
            const auto projectSessionResult = m_LayoutManager.LoadProjectSession();
            if (projectSessionResult.IsSuccess())
            {
                ApplySession(projectSessionResult.GetValue(), visibility, panelState);
                return;
            }

            if (projectSessionResult.GetError().GetCode() != Life::ErrorCode::FileNotFound)
                LOG_WARN("Failed to restore project editor layout session: {}", projectSessionResult.GetError().GetErrorMessage());
        }

        const auto globalSessionResult = m_LayoutManager.LoadGlobalSession();
        if (globalSessionResult.IsSuccess())
        {
            ApplySession(globalSessionResult.GetValue(), visibility, panelState);
            return;
        }

        if (globalSessionResult.GetError().GetCode() != Life::ErrorCode::FileNotFound)
            LOG_WARN("Failed to restore global editor layout session: {}", globalSessionResult.GetError().GetErrorMessage());

        ApplyDefaultLayout(visibility, panelState);
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::ProcessPendingLayoutCommand(EditorPanelVisibility& visibility, EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        const PendingLayoutCommand command = m_PendingLayoutCommand;
        m_PendingLayoutCommand = {};

        switch (command.Type)
        {
        case PendingLayoutCommandType::LoadLayout:
        {
            const auto layoutResult = m_LayoutManager.LoadLayout(command.LayoutId);
            if (layoutResult.IsFailure())
            {
                LOG_WARN("Failed to load editor layout '{}': {}", command.LayoutId.Name, layoutResult.GetError().GetErrorMessage());
                break;
            }

            ApplyLayout(layoutResult.GetValue(), visibility, panelState);
            LOG_INFO("Loaded {} editor layout '{}'.", command.LayoutId.Scope == EditorLayoutScope::Project ? "project" : "global", command.LayoutId.Name);
            break;
        }
        case PendingLayoutCommandType::ApplyDefault:
            ApplyDefaultLayout(visibility, panelState);
            LOG_INFO("Applied default editor layout.");
            break;
        case PendingLayoutCommandType::Revert:
            if (m_ActiveLayout.has_value())
                QueueLoadLayout(*m_ActiveLayout);
            else
                ApplyDefaultLayout(visibility, panelState);
            break;
        case PendingLayoutCommandType::None:
        default:
            break;
        }

        if (m_PendingLayoutCommand.Type == PendingLayoutCommandType::LoadLayout)
            ProcessPendingLayoutCommand(visibility, panelState);
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::QueueLoadLayout(const EditorLayoutId& layoutId)
    {
        m_PendingLayoutCommand.Type = PendingLayoutCommandType::LoadLayout;
        m_PendingLayoutCommand.LayoutId = layoutId;
    }

    void EditorShell::QueueApplyDefaultLayout() noexcept
    {
        m_PendingLayoutCommand = { PendingLayoutCommandType::ApplyDefault, {} };
    }

    void EditorShell::QueueRevertLayout() noexcept
    {
        m_PendingLayoutCommand = { PendingLayoutCommandType::Revert, {} };
    }

    void EditorShell::ApplyDefaultLayout(EditorPanelVisibility& visibility, EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        visibility = EditorLayoutManager::GetDefaultPanelVisibility();
        panelState = {};
        ImGui::ClearIniSettings();
        m_ActiveLayout.reset();
        m_UseDefaultLayout = true;
        m_LayoutInitialized = false;
        BuildDefaultLayout();
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::ApplySession(const EditorLayoutSession& session, EditorPanelVisibility& visibility, EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        visibility = session.PanelVisibility;
        panelState = session.PanelState;
        m_UseDefaultLayout = session.UseDefaultLayout;
        if (session.HasActiveLayout && session.ActiveLayout.IsValid())
            m_ActiveLayout = session.ActiveLayout;
        else
            m_ActiveLayout.reset();

        ImGui::ClearIniSettings();
        if (!session.ImGuiIni.empty())
        {
            ImGui::LoadIniSettingsFromMemory(session.ImGuiIni.c_str(), session.ImGuiIni.size());
            m_LayoutInitialized = true;
        }
        else
        {
            m_LayoutInitialized = false;
            BuildDefaultLayout();
        }
#else
        (void)session;
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::ApplyLayout(const EditorLayoutDefinition& layout, EditorPanelVisibility& visibility, EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        visibility = layout.PanelVisibility;
        panelState = layout.PanelState;
        m_ActiveLayout = EditorLayoutId{ layout.Scope, layout.Name };
        m_UseDefaultLayout = false;
        ImGui::ClearIniSettings();
        if (!layout.ImGuiIni.empty())
        {
            ImGui::LoadIniSettingsFromMemory(layout.ImGuiIni.c_str(), layout.ImGuiIni.size());
            m_LayoutInitialized = true;
        }
        else
        {
            ApplyDefaultLayout(visibility, panelState);
            m_UseDefaultLayout = false;
            m_ActiveLayout = EditorLayoutId{ layout.Scope, layout.Name };
        }
#else
        (void)layout;
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::RenderMenuBar(EditorPanelVisibility& visibility, const EditorPanelState& panelState, EditorShellActions& actions, const FrameContext& context)
    {
#if __has_include(<imgui.h>)
        if (!ImGui::BeginMenuBar())
            return;

        const bool allowSceneShortcuts = context.HasActiveScene && !ImGui::GetIO().WantTextInput;
        if (allowSceneShortcuts)
        {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
                actions.RequestSaveSceneAs = true;
            else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
                actions.RequestSaveScene = true;

            if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))
            {
                if (context.ExecutionMode == EditorSceneExecutionMode::Edit)
                    actions.RequestPlayScene = true;
                else
                    actions.RequestStopScene = true;
            }

            if (ImGui::Shortcut(ImGuiKey_F6, ImGuiInputFlags_RouteGlobal) && context.ExecutionMode == EditorSceneExecutionMode::Edit)
                actions.RequestSimulateScene = true;

            if (ImGui::Shortcut(ImGuiKey_F7, ImGuiInputFlags_RouteGlobal) && context.ExecutionMode != EditorSceneExecutionMode::Edit)
                actions.RequestPauseScene = true;

            if (ImGui::Shortcut(ImGuiKey_F10, ImGuiInputFlags_RouteGlobal) && context.ExecutionMode != EditorSceneExecutionMode::Edit)
                actions.RequestStepScene = true;
        }

        if (ImGui::BeginMenu("Project"))
        {
            if (ImGui::MenuItem("Close Project"))
                actions.RequestCloseProject = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("New Scene"))
                actions.RequestNewScene = true;
            if (ImGui::MenuItem("Open Scene"))
                actions.RequestOpenScene = true;
            if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, context.HasActiveScene))
                actions.RequestSaveScene = true;
            if (ImGui::MenuItem("Save Scene As", "Ctrl+Shift+S", false, context.HasActiveScene))
                actions.RequestSaveSceneAs = true;
            if (ImGui::MenuItem("Close Scene", nullptr, false, context.HasActiveScene))
                actions.RequestCloseScene = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Play", "F5", false, context.HasActiveScene && context.ExecutionMode == EditorSceneExecutionMode::Edit && context.HasSceneCamera))
                actions.RequestPlayScene = true;
            if (ImGui::MenuItem("Simulate", "F6", false, context.HasActiveScene && context.ExecutionMode == EditorSceneExecutionMode::Edit && context.HasSceneCamera))
                actions.RequestSimulateScene = true;
            if (ImGui::MenuItem(context.IsPaused ? "Resume" : "Pause", "F7", false, context.ExecutionMode != EditorSceneExecutionMode::Edit))
                actions.RequestPauseScene = true;
            if (ImGui::MenuItem("Stop", "F5", false, context.ExecutionMode != EditorSceneExecutionMode::Edit))
                actions.RequestStopScene = true;
            if (ImGui::MenuItem("Step", "F10", false, context.ExecutionMode != EditorSceneExecutionMode::Edit))
                actions.RequestStepScene = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Project Assets", nullptr, &visibility.ShowProjectAssets);
            ImGui::MenuItem("Hierarchy", nullptr, &visibility.ShowHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &visibility.ShowInspector);
            ImGui::MenuItem("Console", nullptr, &visibility.ShowConsole);
            ImGui::MenuItem("Stats", nullptr, &visibility.ShowStats);
            ImGui::MenuItem("Scene", nullptr, &visibility.ShowScene);
            ImGui::MenuItem("FPS Overlay", nullptr, &visibility.ShowFpsOverlay);
            ImGui::EndMenu();
        }

        RenderLayoutMenu(visibility, panelState);

        const char* brandLabel = "Life Editor";
        const float brandWidth = ImGui::CalcTextSize(brandLabel).x;
        const float targetCursorX = ImGui::GetWindowContentRegionMax().x - brandWidth - ImGui::GetStyle().ItemSpacing.x;
        if (targetCursorX > ImGui::GetCursorPosX())
            ImGui::SameLine(targetCursorX);
        ImGui::TextColored(ImVec4(0.56f, 0.74f, 1.0f, 1.0f), "%s", brandLabel);
        ImGui::EndMenuBar();
#else
        (void)visibility;
        (void)panelState;
        (void)actions;
        (void)context;
#endif
    }

    void EditorShell::RenderWorkspaceChrome(EditorShellActions& actions, const FrameContext& context) const
    {
#if __has_include(<imgui.h>)
        constexpr float chromeHeight = 42.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        if (ImGui::BeginChild("##EditorWorkspaceChrome", ImVec2(0.0f, chromeHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            auto drawChip = [](const char* label, const char* value, const ImVec4& color)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + 0.04f, color.y + 0.04f, color.z + 0.04f, color.w));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x + 0.02f, color.y + 0.02f, color.z + 0.02f, color.w));
                ImGui::Button(label);
                ImGui::PopStyleColor(3);
                ImGui::SameLine();
                ImGui::TextUnformatted(value);
            };

            const float centeredCursorY = std::max(0.0f, (ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight()) * 0.5f - 2.0f);
            ImGui::SetCursorPosY(centeredCursorY);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Workspace");

            ImGui::SameLine();
            drawChip("Project", (context.ActiveProjectName != nullptr && context.ActiveProjectName[0] != '\0') ? context.ActiveProjectName : "No Project", ImVec4(0.16f, 0.28f, 0.46f, 1.0f));

            ImGui::SameLine();
            const char* sceneLabel = (context.ActiveSceneName != nullptr && context.ActiveSceneName[0] != '\0') ? context.ActiveSceneName : "No Scene";
            drawChip("Scene", sceneLabel, context.IsSceneDirty ? ImVec4(0.36f, 0.26f, 0.08f, 1.0f) : ImVec4(0.17f, 0.25f, 0.20f, 1.0f));

            ImGui::SameLine();
            const ImVec4 modeColor = context.ExecutionMode == EditorSceneExecutionMode::Play
                ? ImVec4(0.18f, 0.34f, 0.18f, 1.0f)
                : context.ExecutionMode == EditorSceneExecutionMode::Simulation
                    ? ImVec4(0.30f, 0.25f, 0.12f, 1.0f)
                    : ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
            drawChip("Mode", ResolveExecutionModeLabel(context.ExecutionMode), modeColor);

            ImGui::SameLine();
            const bool canPlay = context.HasActiveScene && context.HasSceneCamera;
            const bool isEditMode = context.ExecutionMode == EditorSceneExecutionMode::Edit;
            if (ImGui::Button(isEditMode ? "Play" : "Stop") && canPlay)
            {
                if (isEditMode)
                    actions.RequestPlayScene = true;
                else
                    actions.RequestStopScene = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Simulate") && context.ExecutionMode == EditorSceneExecutionMode::Edit && canPlay)
                actions.RequestSimulateScene = true;

            ImGui::SameLine();
            if (ImGui::Button(context.IsPaused ? "Resume" : "Pause") && context.ExecutionMode != EditorSceneExecutionMode::Edit)
                actions.RequestPauseScene = true;

            ImGui::SameLine();
            if (ImGui::Button("Step") && context.ExecutionMode != EditorSceneExecutionMode::Edit)
                actions.RequestStepScene = true;

            if (context.IsSceneDirty)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.73f, 0.30f, 1.0f), "Unsaved changes");
            }

            if (!context.HasSceneCamera)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "No scene camera");
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(4);
#else
        (void)actions;
        (void)context;
#endif
    }

    void EditorShell::RenderLayoutMenu(EditorPanelVisibility& visibility, const EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        if (!ImGui::BeginMenu("Layout"))
            return;

        if (ImGui::MenuItem("Default Layout"))
            QueueApplyDefaultLayout();

        if (ImGui::MenuItem("Revert Layout", nullptr, false, m_UseDefaultLayout || m_ActiveLayout.has_value()))
            QueueRevertLayout();

        if (ImGui::MenuItem("Save Layout"))
        {
            if (m_ActiveLayout.has_value())
            {
                const auto snapshot = CaptureLayoutSnapshot(visibility, panelState);
                if (snapshot.has_value())
                {
                    EditorLayoutDefinition layout;
                    layout.Name = m_ActiveLayout->Name;
                    layout.Scope = m_ActiveLayout->Scope;
                    layout.PanelVisibility = snapshot->PanelVisibility;
                    layout.PanelState = snapshot->PanelState;
                    layout.ImGuiIni = snapshot->ImGuiIni;
                    HandleResult("save editor layout", m_LayoutManager.SaveLayout(layout));
                }
            }
            else
            {
                OpenSaveLayoutAsDialog(m_LayoutManager.HasProjectScope() ? EditorLayoutScope::Project : EditorLayoutScope::Global);
            }
        }

        if (ImGui::MenuItem("Save Layout As..."))
            OpenSaveLayoutAsDialog(m_ActiveLayout.has_value() ? m_ActiveLayout->Scope : (m_LayoutManager.HasProjectScope() ? EditorLayoutScope::Project : EditorLayoutScope::Global));

        if (ImGui::BeginMenu("Load Global Layout"))
        {
            const auto layouts = m_LayoutManager.ListLayouts(EditorLayoutScope::Global);
            if (layouts.empty())
                ImGui::MenuItem("No Global Layouts", nullptr, false, false);
            for (const auto& layout : layouts)
            {
                if (ImGui::MenuItem(layout.Id.Name.c_str()))
                    QueueLoadLayout(layout.Id);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Load Project Layout", m_LayoutManager.HasProjectScope()))
        {
            const auto layouts = m_LayoutManager.ListLayouts(EditorLayoutScope::Project);
            if (layouts.empty())
                ImGui::MenuItem("No Project Layouts", nullptr, false, false);
            for (const auto& layout : layouts)
            {
                if (ImGui::MenuItem(layout.Id.Name.c_str()))
                    QueueLoadLayout(layout.Id);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Delete Global Layout"))
        {
            const auto layouts = m_LayoutManager.ListLayouts(EditorLayoutScope::Global);
            if (layouts.empty())
                ImGui::MenuItem("No Global Layouts", nullptr, false, false);
            for (const auto& layout : layouts)
            {
                if (ImGui::MenuItem(layout.Id.Name.c_str()))
                {
                    m_DeleteLayoutTarget = layout.Id;
                    m_OpenDeleteLayoutPopup = true;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Delete Project Layout", m_LayoutManager.HasProjectScope()))
        {
            const auto layouts = m_LayoutManager.ListLayouts(EditorLayoutScope::Project);
            if (layouts.empty())
                ImGui::MenuItem("No Project Layouts", nullptr, false, false);
            for (const auto& layout : layouts)
            {
                if (ImGui::MenuItem(layout.Id.Name.c_str()))
                {
                    m_DeleteLayoutTarget = layout.Id;
                    m_OpenDeleteLayoutPopup = true;
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::RenderLayoutDialogs(EditorPanelVisibility& visibility, const EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        if (m_OpenSaveLayoutAsPopup)
        {
            ImGui::OpenPopup("Save Layout As");
            m_OpenSaveLayoutAsPopup = false;
        }
        if (m_OpenDeleteLayoutPopup)
        {
            ImGui::OpenPopup("Delete Layout");
            m_OpenDeleteLayoutPopup = false;
        }

        if (ImGui::BeginPopupModal("Save Layout As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Layout Name", m_SaveLayoutName);
            int layoutScope = m_SaveLayoutScope == EditorLayoutScope::Project ? 1 : 0;
            if (ImGui::RadioButton("Global", layoutScope == 0))
                layoutScope = 0;
            if (m_LayoutManager.HasProjectScope())
            {
                ImGui::SameLine();
                if (ImGui::RadioButton("Project", layoutScope == 1))
                    layoutScope = 1;
            }
            m_SaveLayoutScope = layoutScope == 1 && m_LayoutManager.HasProjectScope() ? EditorLayoutScope::Project : EditorLayoutScope::Global;

            if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
            {
                const std::string layoutName = EditorLayoutManager::SanitizeLayoutName(m_SaveLayoutName);
                const auto snapshot = CaptureLayoutSnapshot(visibility, panelState);
                if (layoutName.empty())
                {
                    LOG_WARN("Editor layout name must not be empty.");
                }
                else if (snapshot.has_value())
                {
                    EditorLayoutDefinition layout;
                    layout.Name = layoutName;
                    layout.Scope = m_SaveLayoutScope;
                    layout.PanelVisibility = snapshot->PanelVisibility;
                    layout.PanelState = snapshot->PanelState;
                    layout.ImGuiIni = snapshot->ImGuiIni;

                    const auto saveResult = m_LayoutManager.SaveLayout(layout);
                    if (saveResult.IsFailure())
                    {
                        LOG_WARN("Failed to save editor layout '{}': {}", layout.Name, saveResult.GetError().GetErrorMessage());
                    }
                    else
                    {
                        m_ActiveLayout = EditorLayoutId{ layout.Scope, layout.Name };
                        m_UseDefaultLayout = false;
                        LOG_INFO("Saved {} editor layout '{}'.", layout.Scope == EditorLayoutScope::Project ? "project" : "global", layout.Name);
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Delete Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (m_DeleteLayoutTarget.has_value())
                ImGui::Text("Delete %s layout '%s'?", m_DeleteLayoutTarget->Scope == EditorLayoutScope::Project ? "project" : "global", m_DeleteLayoutTarget->Name.c_str());

            if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
            {
                if (m_DeleteLayoutTarget.has_value())
                {
                    const EditorLayoutId deletedLayout = *m_DeleteLayoutTarget;
                    const auto deleteResult = m_LayoutManager.DeleteLayout(deletedLayout);
                    if (deleteResult.IsFailure())
                    {
                        LOG_WARN("Failed to delete editor layout '{}': {}", deletedLayout.Name, deleteResult.GetError().GetErrorMessage());
                    }
                    else
                    {
                        if (m_ActiveLayout.has_value() && m_ActiveLayout->Scope == deletedLayout.Scope && m_ActiveLayout->Name == deletedLayout.Name)
                            m_ActiveLayout.reset();
                        m_DeleteLayoutTarget.reset();
                        LOG_INFO("Deleted {} editor layout '{}'.", deletedLayout.Scope == EditorLayoutScope::Project ? "project" : "global", deletedLayout.Name);
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            {
                m_DeleteLayoutTarget.reset();
                ImGui::CloseCurrentPopup();
            }

            if (!ImGui::IsPopupOpen("Delete Layout"))
                m_DeleteLayoutTarget.reset();

            ImGui::EndPopup();
        }
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    std::optional<EditorShell::LayoutSnapshot> EditorShell::CaptureLayoutSnapshot(const EditorPanelVisibility& visibility, const EditorPanelState& panelState) const
    {
#if __has_include(<imgui.h>)
        size_t iniSize = 0;
        const char* iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
        if (iniData == nullptr)
            return std::nullopt;

        LayoutSnapshot snapshot;
        snapshot.PanelVisibility = visibility;
        snapshot.PanelState = panelState;
        snapshot.ImGuiIni.assign(iniData, iniSize);
        return snapshot;
#else
        (void)visibility;
        (void)panelState;
        return std::nullopt;
#endif
    }

    void EditorShell::PersistLayoutSessions(const EditorPanelVisibility& visibility, const EditorPanelState& panelState)
    {
#if __has_include(<imgui.h>)
        const auto snapshot = CaptureLayoutSnapshot(visibility, panelState);
        if (!snapshot.has_value())
            return;

        const double currentTime = ImGui::GetTime();
        const bool globalDirty = !m_LastPersistedGlobalSession.has_value() || m_LastPersistedGlobalSession.value() != snapshot.value();
        const bool projectDirty = m_LayoutManager.HasProjectScope() && (!m_LastPersistedProjectSession.has_value() || m_LastPersistedProjectSession.value() != snapshot.value());
        if (!globalDirty && !projectDirty)
            return;

        if (currentTime < m_NextPersistTime)
            return;

        EditorLayoutSession session;
        session.PanelVisibility = snapshot->PanelVisibility;
        session.PanelState = snapshot->PanelState;
        session.ImGuiIni = snapshot->ImGuiIni;
        session.UseDefaultLayout = m_UseDefaultLayout;
        session.HasActiveLayout = m_ActiveLayout.has_value();
        if (m_ActiveLayout.has_value())
            session.ActiveLayout = *m_ActiveLayout;

        const auto globalSaveResult = m_LayoutManager.SaveGlobalSession(session);
        if (globalSaveResult.IsSuccess())
            m_LastPersistedGlobalSession = snapshot;
        else
            LOG_WARN("Failed to persist global editor layout session: {}", globalSaveResult.GetError().GetErrorMessage());

        if (m_LayoutManager.HasProjectScope())
        {
            const auto projectSaveResult = m_LayoutManager.SaveProjectSession(session);
            if (projectSaveResult.IsSuccess())
                m_LastPersistedProjectSession = snapshot;
            else
                LOG_WARN("Failed to persist project editor layout session: {}", projectSaveResult.GetError().GetErrorMessage());
        }

        m_NextPersistTime = currentTime + 0.75;
#else
        (void)visibility;
        (void)panelState;
#endif
    }

    void EditorShell::HandleResult(const char* action, const Life::Result<void>& result) const
    {
        if (result.IsFailure())
            LOG_WARN("Failed to {}: {}", action, result.GetError().GetErrorMessage());
        else
            LOG_INFO("{} succeeded.", action);
    }

    void EditorShell::OpenSaveLayoutAsDialog(EditorLayoutScope preferredScope)
    {
        m_SaveLayoutName = m_ActiveLayout.has_value() ? m_ActiveLayout->Name : "Layout";
        m_SaveLayoutScope = preferredScope == EditorLayoutScope::Project && m_LayoutManager.HasProjectScope()
            ? EditorLayoutScope::Project
            : EditorLayoutScope::Global;
        m_OpenSaveLayoutAsPopup = true;
    }
}
