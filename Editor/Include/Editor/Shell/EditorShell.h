#pragma once

#include "Editor/Shell/EditorLayoutManager.h"
#include "Editor/Shell/EditorShellTypes.h"

#include <optional>
#include <string>

namespace Life::Assets
{
    struct Project;
}

namespace EditorApp
{
    class EditorShell
    {
    public:
        struct FrameContext
        {
            const char* ActiveProjectName = nullptr;
            const char* ActiveSceneName = nullptr;
            const Life::Assets::Project* ActiveProject = nullptr;
            bool HasActiveScene = false;
            bool IsSceneDirty = false;
        };

        void ResetLayout() noexcept;
        void Begin(EditorPanelVisibility& visibility, EditorPanelState& panelState, EditorShellActions& actions, const FrameContext& context);
        void End(const EditorPanelVisibility& visibility, const EditorPanelState& panelState);

    private:
        struct LayoutSnapshot
        {
            EditorPanelVisibility PanelVisibility;
            EditorPanelState PanelState;
            std::string ImGuiIni;

            bool operator==(const LayoutSnapshot& other) const noexcept
            {
                return PanelVisibility == other.PanelVisibility && PanelState == other.PanelState && ImGuiIni == other.ImGuiIni;
            }
        };

        enum class PendingLayoutCommandType
        {
            None,
            LoadLayout,
            ApplyDefault,
            Revert
        };

        struct PendingLayoutCommand
        {
            PendingLayoutCommandType Type = PendingLayoutCommandType::None;
            EditorLayoutId LayoutId;
        };

        void BuildDefaultLayout();
        void UpdateProjectContext(const FrameContext& context);
        void RestoreStartupLayout(EditorPanelVisibility& visibility, EditorPanelState& panelState);
        void ProcessPendingLayoutCommand(EditorPanelVisibility& visibility, EditorPanelState& panelState);
        void QueueLoadLayout(const EditorLayoutId& layoutId);
        void QueueApplyDefaultLayout() noexcept;
        void QueueRevertLayout() noexcept;
        void ApplyDefaultLayout(EditorPanelVisibility& visibility, EditorPanelState& panelState);
        void ApplySession(const EditorLayoutSession& session, EditorPanelVisibility& visibility, EditorPanelState& panelState);
        void ApplyLayout(const EditorLayoutDefinition& layout, EditorPanelVisibility& visibility, EditorPanelState& panelState);
        void RenderMenuBar(EditorPanelVisibility& visibility, const EditorPanelState& panelState, EditorShellActions& actions, const FrameContext& context);
        void RenderWorkspaceChrome(const FrameContext& context) const;
        void RenderLayoutMenu(EditorPanelVisibility& visibility, const EditorPanelState& panelState);
        void RenderLayoutDialogs(EditorPanelVisibility& visibility, const EditorPanelState& panelState);
        std::optional<LayoutSnapshot> CaptureLayoutSnapshot(const EditorPanelVisibility& visibility, const EditorPanelState& panelState) const;
        void PersistLayoutSessions(const EditorPanelVisibility& visibility, const EditorPanelState& panelState);
        void HandleResult(const char* action, const Life::Result<void>& result) const;
        void OpenSaveLayoutAsDialog(EditorLayoutScope preferredScope);

        EditorLayoutManager m_LayoutManager;
        bool m_LayoutInitialized = false;
        bool m_StartupLayoutResolved = false;
        bool m_UseDefaultLayout = true;
        double m_NextPersistTime = 0.0;
        std::optional<EditorLayoutId> m_ActiveLayout;
        std::optional<LayoutSnapshot> m_LastPersistedProjectSession;
        std::optional<LayoutSnapshot> m_LastPersistedGlobalSession;
        PendingLayoutCommand m_PendingLayoutCommand;
        std::string m_SaveLayoutName = "Layout";
        EditorLayoutScope m_SaveLayoutScope = EditorLayoutScope::Global;
        bool m_OpenSaveLayoutAsPopup = false;
        std::optional<EditorLayoutId> m_DeleteLayoutTarget;
        bool m_OpenDeleteLayoutPopup = false;
    };
}
