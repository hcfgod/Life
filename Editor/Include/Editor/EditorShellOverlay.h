#pragma once

#include "Engine.h"

#include <memory>
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
        void DrawSceneSurfaceContent(Life::Renderer2D& renderer2D);
        Life::Camera* TryGetEditorCamera();
        bool RenderSceneSurface(uint32_t width, uint32_t height);

        bool m_ShowHierarchyPanel = true;
        bool m_ShowInspectorPanel = true;
        bool m_ShowConsolePanel = true;
        bool m_ShowStatsPanel = true;
        bool m_ShowScenePanel = true;
        bool m_LayoutInitialized = false;
        bool m_OwnsCamera = false;
        float m_ElapsedTime = 0.0f;
        std::string m_EditorCameraName = "EditorSceneCamera";
        std::string m_CheckerTextureKey = "Assets/Textures/Renderer2DChecker.ppm";
        std::shared_ptr<Life::Assets::TextureAsset> m_CheckerTextureAsset;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
    };
}
