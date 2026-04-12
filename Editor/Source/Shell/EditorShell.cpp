#include "Editor/Shell/EditorShell.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#include <imgui_internal.h>
#endif

namespace EditorApp
{
    void EditorShell::ResetLayout() noexcept
    {
        m_LayoutInitialized = false;
    }

    void EditorShell::Begin(EditorPanelVisibility& visibility, EditorShellActions& actions, const FrameContext& context)
    {
#if __has_include(<imgui.h>)
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

        BuildDefaultLayout();
        RenderMenuBar(visibility, actions, context);

        const ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
#else
        (void)visibility;
        (void)actions;
        (void)context;
#endif
    }

    void EditorShell::End()
    {
#if __has_include(<imgui.h>)
        ImGui::End();
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
        ImGuiID leftDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Left, 0.20f, nullptr, &rootDockId);
        ImGuiID rightDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Right, 0.24f, nullptr, &rootDockId);
        ImGuiID bottomDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Down, 0.28f, nullptr, &rootDockId);

        ImGui::DockBuilderDockWindow("Hierarchy", leftDockId);
        ImGui::DockBuilderDockWindow("Inspector", rightDockId);
        ImGui::DockBuilderDockWindow("Renderer Stress", rightDockId);
        ImGui::DockBuilderDockWindow("Console", bottomDockId);
        ImGui::DockBuilderDockWindow("Stats", rightDockId);
        ImGui::DockBuilderDockWindow("Scene", rootDockId);
        ImGui::DockBuilderFinish(dockspaceId);
#endif
    }

    void EditorShell::RenderMenuBar(EditorPanelVisibility& visibility, EditorShellActions& actions, const FrameContext& context)
    {
#if __has_include(<imgui.h>)
        if (!ImGui::BeginMenuBar())
            return;

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("New Scene"))
                actions.RequestNewScene = true;
            if (ImGui::MenuItem("Open Scene"))
                actions.RequestOpenScene = true;
            if (ImGui::MenuItem("Save Scene", nullptr, false, context.HasActiveScene))
                actions.RequestSaveScene = true;
            if (ImGui::MenuItem("Save Scene As", nullptr, false, context.HasActiveScene))
                actions.RequestSaveSceneAs = true;
            if (ImGui::MenuItem("Close Scene", nullptr, false, context.HasActiveScene))
                actions.RequestCloseScene = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Project"))
        {
            if (ImGui::MenuItem("Close Project"))
                actions.RequestCloseProject = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Hierarchy", nullptr, &visibility.ShowHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &visibility.ShowInspector);
            ImGui::MenuItem("Console", nullptr, &visibility.ShowConsole);
            ImGui::MenuItem("Renderer Stress", nullptr, &visibility.ShowRendererStress);
            ImGui::MenuItem("Stats", nullptr, &visibility.ShowStats);
            ImGui::MenuItem("Scene", nullptr, &visibility.ShowScene);
            ImGui::MenuItem("FPS Overlay", nullptr, &visibility.ShowFpsOverlay);
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Life Editor");
        if (context.ActiveProjectName != nullptr && context.ActiveProjectName[0] != '\0')
        {
            ImGui::Separator();
            ImGui::TextUnformatted(context.ActiveProjectName);
        }
        if (context.ActiveSceneName != nullptr && context.ActiveSceneName[0] != '\0')
        {
            ImGui::Separator();
            ImGui::Text("%s%s", context.ActiveSceneName, context.IsSceneDirty ? " *" : "");
        }
        ImGui::EndMenuBar();
#else
        (void)visibility;
        (void)actions;
        (void)context;
#endif
    }
}
