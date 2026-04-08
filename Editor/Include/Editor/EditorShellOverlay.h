#pragma once

#include "Engine.h"

#include <functional>
#include <memory>
#include <optional>
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
        void TryAcquireCheckerTexture();
        void CacheServices();
        void ReleaseCachedServices() noexcept;
        void EnsureEditorCamera();
        void DrawSceneSurfaceContent(Life::Renderer2D& renderer2D);
        Life::OptionalRef<Life::Camera> TryGetEditorCamera();
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
        Life::OptionalRef<Life::Application> m_Application;
        Life::OptionalRef<Life::InputSystem> m_InputSystem;
        Life::OptionalRef<Life::Assets::AssetManager> m_AssetManager;
        Life::OptionalRef<Life::CameraManager> m_CameraManager;
        Life::OptionalRef<Life::Renderer> m_Renderer;
        Life::OptionalRef<Life::Renderer2D> m_Renderer2D;
        Life::OptionalRef<Life::ImGuiSystem> m_ImGuiSystem;
        Life::Ref<Life::Assets::TextureAsset> m_CheckerTextureAsset;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
    };
}
