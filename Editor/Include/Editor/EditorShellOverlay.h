#pragma once

#include "Engine.h"
#include "Graphics/TextureResource.h"

#include <string>

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
        void EnsureEditorCamera();
        void EnsureSceneRenderTarget(uint32_t width, uint32_t height);
        void ReleaseSceneRenderTarget() noexcept;
        void RenderSceneViewport(Life::ImGuiSystem& imguiSystem);

        bool m_ShowHierarchyPanel = true;
        bool m_ShowInspectorPanel = true;
        bool m_ShowConsolePanel = true;
        bool m_ShowStatsPanel = true;
        bool m_ShowScenePanel = true;
        bool m_LayoutInitialized = false;
        bool m_OwnsCamera = false;
        bool m_LoggedSceneTargetFailure = false;
        bool m_LoggedSceneHandleFailure = false;
        bool m_LoggedSceneViewportReady = false;
        float m_ElapsedTime = 0.0f;
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;
        std::string m_EditorCameraName = "EditorSceneCamera";
        Life::Scope<Life::TextureResource> m_SceneColorTarget;
        void* m_SceneTextureHandle = nullptr;
    };
}
