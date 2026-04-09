#pragma once

#include "Editor/EditorServices.h"
#include "Engine.h"

namespace EditorApp
{
    class EditorCameraTool;

    struct SceneViewportState
    {
        bool SurfaceReady = false;
        bool LastRenderSucceeded = false;
        uint32_t SurfaceWidth = 0;
        uint32_t SurfaceHeight = 0;
        Life::Renderer2D::Statistics RendererStats{};
    };

    class SceneViewportPanel
    {
    public:
        explicit SceneViewportPanel(std::string checkerTextureKey = "Assets/Textures/Renderer2DChecker.ppm");

        void Attach(const EditorServices& services);
        void Detach() noexcept;
        void Update(const EditorServices& services, float timestep);
        void Render(bool& isOpen, const EditorServices& services, EditorCameraTool& cameraTool);

        const SceneViewportState& GetState() const noexcept;

    private:
        void TryAcquireCheckerTexture(const EditorServices& services);
        Life::SceneRenderer2D::Scene2D BuildScene2D(const Life::Camera& camera) const;
        bool RenderSceneSurface(uint32_t width, uint32_t height, const EditorServices& services, EditorCameraTool& cameraTool);

        std::string m_CheckerTextureKey;
        float m_ElapsedTime = 0.0f;
        Life::Ref<Life::Assets::TextureAsset> m_CheckerTextureAsset;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
        SceneViewportState m_State;
    };
}
