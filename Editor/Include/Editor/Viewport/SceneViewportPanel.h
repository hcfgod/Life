#pragma once

#include "Editor/EditorServices.h"
#include "Editor/Scene/EditorSceneState.h"
#include "Editor/Undo/EditorUndoStack.h"
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
        uint32_t RequestedRenderWidth = 0;
        uint32_t RequestedRenderHeight = 0;
        uint32_t BackBufferWidth = 0;
        uint32_t BackBufferHeight = 0;
        float DisplayWidth = 0.0f;
        float DisplayHeight = 0.0f;
        float FramebufferScaleX = 1.0f;
        float FramebufferScaleY = 1.0f;
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
        void Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool, EditorUndoStack& undoStack);

        const SceneViewportState& GetState() const noexcept;

    private:
        void UpdateCameraNavigation(EditorCameraTool& cameraTool, Life::Camera& camera, bool viewportHovered, bool viewportFocused);
        void SetCameraNavigationActive(bool active) noexcept;
        bool RenderSceneSurface(uint32_t renderWidth,
                                uint32_t renderHeight,
                                float displayWidth,
                                float displayHeight,
                                float viewportScreenX,
                                float viewportScreenY,
                                const EditorServices& services,
                                EditorSceneState& sceneState,
                                EditorCameraTool& cameraTool,
                                EditorUndoStack& undoStack,
                                bool viewportHovered,
                                bool viewportFocused);

        float m_LastTimestep = 0.0f;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
        SceneViewportState m_State;
        void* m_NativeWindowHandle = nullptr;
        bool m_CameraNavigationActive = false;
        bool m_GizmoManipulating = false;
        std::string m_GizmoEntityId;
        Life::TransformComponent m_GizmoTransformBefore{};
    };
}
