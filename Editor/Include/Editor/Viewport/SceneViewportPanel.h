#pragma once

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorSceneState.h"
#include "Engine.h"

namespace EditorApp
{
    class EditorCameraTool;

    struct SceneViewportState
    {
        bool SurfaceReady = false;
        bool LastRenderSucceeded = false;
        bool UsingEditorCamera = true;
        bool UsingSceneCamera = false;
        EditorSceneExecutionMode ExecutionMode = EditorSceneExecutionMode::Edit;
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
        SceneViewportPanel() = default;

        void Attach(const EditorServices& services);
        void Detach() noexcept;
        void Update(const EditorServices& services, float timestep);
        void Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool);

        const SceneViewportState& GetState() const noexcept;

    private:
        void UpdateCameraNavigation(EditorCameraTool& cameraTool, Life::Camera& camera, bool viewportHovered, bool viewportFocused);
        void SetCameraNavigationActive(bool active) noexcept;
        bool RenderSceneSurface(uint32_t width, uint32_t height, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool, bool viewportHovered, bool viewportFocused);

        float m_LastTimestep = 0.0f;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
        SceneViewportState m_State;
        void* m_NativeWindowHandle = nullptr;
        bool m_CameraNavigationActive = false;
    };
}
