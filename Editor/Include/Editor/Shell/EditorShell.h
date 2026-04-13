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
        void Begin(EditorPanelVisibility& visibility, EditorShellActions& actions, const FrameContext& context);
        void End(const EditorPanelVisibility& visibility);

    private:
        struct LayoutSnapshot
        {
            EditorPanelVisibility PanelVisibility;
            std::string ImGuiIni;

            bool operator==(const LayoutSnapshot& other) const noexcept
            {
                return PanelVisibility == other.PanelVisibility && ImGuiIni == other.ImGuiIni;
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
        void RestoreStartupLayout(EditorPanelVisibility& visibility);
        void ProcessPendingLayoutCommand(EditorPanelVisibility& visibility);
        void QueueLoadLayout(const EditorLayoutId& layoutId);
        void QueueApplyDefaultLayout() noexcept;
        void QueueRevertLayout() noexcept;
        void ApplyDefaultLayout(EditorPanelVisibility& visibility);
        void ApplySession(const EditorLayoutSession& session, EditorPanelVisibility& visibility);
        void ApplyLayout(const EditorLayoutDefinition& layout, EditorPanelVisibility& visibility);
        void RenderMenuBar(EditorPanelVisibility& visibility, EditorShellActions& actions, const FrameContext& context);
        void RenderLayoutMenu(EditorPanelVisibility& visibility);
        void RenderLayoutDialogs(EditorPanelVisibility& visibility);
        std::optional<LayoutSnapshot> CaptureLayoutSnapshot(const EditorPanelVisibility& visibility) const;
        void PersistLayoutSessions(const EditorPanelVisibility& visibility);
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
