#pragma once

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorSceneState.h"
#include "Engine.h"

namespace EditorApp
{
    class EditorCameraTool;

    struct SceneStressTestSettings
    {
        bool Enabled = true;
        uint32_t Columns = 24;
        uint32_t Rows = 18;
        glm::vec2 QuadSize{ 0.42f, 0.42f };
        float Spacing = 0.55f;
        bool DrawTexturedQuads = true;
        bool DrawColoredQuads = true;
        float TexturedMix = 0.5f;
        bool Animate = true;
        float MotionAmplitude = 0.12f;
        float RotationSpeed = 0.35f;
        float DepthStep = 0.001f;
    };

    struct SceneViewportState
    {
        bool SurfaceReady = false;
        bool LastRenderSucceeded = false;
        uint32_t SurfaceWidth = 0;
        uint32_t SurfaceHeight = 0;
        uint32_t RequestedQuadCount = 0;
        uint32_t TexturedQuadCount = 0;
        uint32_t UntexturedQuadCount = 0;
        Life::Renderer2D::Statistics RendererStats{};
    };

    class SceneViewportPanel
    {
    public:
        explicit SceneViewportPanel(std::string checkerTextureKey = "Assets/Textures/Renderer2DChecker.ppm");

        void Attach(const EditorServices& services);
        void Detach() noexcept;
        void Update(const EditorServices& services, float timestep);
        void Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool);
        void RenderStressPanel(bool& isOpen);

        const SceneViewportState& GetState() const noexcept;

    private:
        void TryAcquireCheckerTexture(const EditorServices& services);
        void RenderStressControls();
        void UpdateCameraNavigation(EditorCameraTool& cameraTool, Life::Camera& camera, bool viewportHovered, bool viewportFocused);
        void SetCameraNavigationActive(bool active) noexcept;
        uint32_t GetConfiguredQuadCount() const noexcept;
        Life::SceneRenderer2D::Scene2D BuildScene2D(const Life::Camera& camera);
        bool RenderSceneSurface(uint32_t width, uint32_t height, const EditorServices& services, EditorCameraTool& cameraTool, bool viewportHovered, bool viewportFocused);

        std::string m_CheckerTextureKey;
        float m_ElapsedTime = 0.0f;
        float m_LastTimestep = 0.0f;
        Life::Ref<Life::Assets::TextureAsset> m_CheckerTextureAsset;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
        SceneStressTestSettings m_StressSettings;
        SceneViewportState m_State;
        void* m_NativeWindowHandle = nullptr;
        bool m_CameraNavigationActive = false;
    };
}
