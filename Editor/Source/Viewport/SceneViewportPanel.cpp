#include "Editor/Viewport/SceneViewportPanel.h"

#include "Editor/Camera/EditorCameraTool.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#include <SDL3/SDL.h>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif
#if __has_include(<ImGuizmo.h>)
#include <ImGuizmo.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include <glm/gtc/type_ptr.hpp>

namespace
{
    float SnapToPixelGrid(float value, float pixelScale)
    {
        const float safePixelScale = std::max(pixelScale, 1.0f);
        return std::round(value * safePixelScale) / safePixelScale;
    }

    ImVec2 ResolveFramebufferScale(const EditorApp::EditorServices& services, void* nativeWindowHandle)
    {
#if __has_include(<imgui.h>)
        const ImVec2 reportedScale = ImGui::GetIO().DisplayFramebufferScale;
        float scaleX = std::max(reportedScale.x, 1.0f);
        float scaleY = std::max(reportedScale.y, 1.0f);

        if (auto* sdlWindow = static_cast<SDL_Window*>(nativeWindowHandle))
        {
            int windowWidth = 0;
            int windowHeight = 0;
            int pixelWidth = 0;
            int pixelHeight = 0;
            if (SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight) &&
                SDL_GetWindowSizeInPixels(sdlWindow, &pixelWidth, &pixelHeight) &&
                windowWidth > 0 &&
                windowHeight > 0 &&
                pixelWidth > 0 &&
                pixelHeight > 0)
            {
                scaleX = static_cast<float>(pixelWidth) / static_cast<float>(windowWidth);
                scaleY = static_cast<float>(pixelHeight) / static_cast<float>(windowHeight);
            }
            else
            {
                const float pixelDensity = SDL_GetWindowPixelDensity(sdlWindow);
                if (std::isfinite(pixelDensity) && pixelDensity > 0.0f)
                {
                    scaleX = pixelDensity;
                    scaleY = pixelDensity;
                }
            }
        }

        if (services.GraphicsDevice)
        {
            if (const ImGuiViewport* viewport = ImGui::GetWindowViewport())
            {
                const float viewportWidth = viewport->Size.x;
                const float viewportHeight = viewport->Size.y;
                if (viewportWidth > 0.0f && viewportHeight > 0.0f)
                {
                    const Life::GraphicsDevice& graphicsDevice = services.GraphicsDevice->get();
                    const float derivedScaleX = static_cast<float>(graphicsDevice.GetBackBufferWidth()) / viewportWidth;
                    const float derivedScaleY = static_cast<float>(graphicsDevice.GetBackBufferHeight()) / viewportHeight;
                    if (std::isfinite(derivedScaleX) && derivedScaleX >= scaleX)
                        scaleX = derivedScaleX;
                    if (std::isfinite(derivedScaleY) && derivedScaleY >= scaleY)
                        scaleY = derivedScaleY;
                }
            }
        }

        return { scaleX, scaleY };
#else
        (void)services;
        return { 1.0f, 1.0f };
#endif
    }

    glm::vec2 GetCameraLookDelta(void* nativeWindowHandle)
    {
        if (auto* sdlWindow = static_cast<SDL_Window*>(nativeWindowHandle))
        {
            if (SDL_GetWindowRelativeMouseMode(sdlWindow))
            {
                float deltaX = 0.0f;
                float deltaY = 0.0f;
                SDL_GetRelativeMouseState(&deltaX, &deltaY);
                return { deltaX, deltaY };
            }
        }

#if __has_include(<imgui.h>)
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        return { mouseDelta.x, mouseDelta.y };
#else
        return {};
#endif
    }

    void SetCameraNavigationMouseRect(void* nativeWindowHandle, const ImVec2& viewportOrigin, const ImVec2& viewportSize)
    {
        if (auto* sdlWindow = static_cast<SDL_Window*>(nativeWindowHandle))
        {
            SDL_Rect rect{};
            rect.x = std::max(0, static_cast<int>(std::floor(viewportOrigin.x)));
            rect.y = std::max(0, static_cast<int>(std::floor(viewportOrigin.y)));
            rect.w = std::max(1, static_cast<int>(std::ceil(viewportSize.x)));
            rect.h = std::max(1, static_cast<int>(std::ceil(viewportSize.y)));
            SDL_SetWindowMouseRect(sdlWindow, &rect);
        }
    }

#if __has_include(<imgui.h>)
    const char* ResolveViewportToolLabel(EditorApp::EditorViewportTool tool) noexcept
    {
        switch (tool)
        {
            case EditorApp::EditorViewportTool::Select: return "Select";
            case EditorApp::EditorViewportTool::Rotate: return "Rotate";
            case EditorApp::EditorViewportTool::Scale: return "Scale";
            case EditorApp::EditorViewportTool::Translate:
            default: return "Move";
        }
    }

    void DrawToolButton(const char* label, EditorApp::EditorViewportTool value, EditorApp::EditorSceneState& sceneState)
    {
        const bool selected = sceneState.ViewportTool == value;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.42f, 0.70f, 1.0f));
        if (ImGui::Button(label, ImVec2(68.0f, 0.0f)))
            sceneState.ViewportTool = value;
        if (selected)
            ImGui::PopStyleColor();
    }

    void DrawViewportToolbar(EditorApp::EditorSceneState& sceneState)
    {
        DrawToolButton("Select", EditorApp::EditorViewportTool::Select, sceneState);
        ImGui::SameLine();
        DrawToolButton("Move", EditorApp::EditorViewportTool::Translate, sceneState);
        ImGui::SameLine();
        DrawToolButton("Rotate", EditorApp::EditorViewportTool::Rotate, sceneState);
        ImGui::SameLine();
        DrawToolButton("Scale", EditorApp::EditorViewportTool::Scale, sceneState);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(82.0f);
        if (ImGui::BeginCombo("##TransformSpace", sceneState.TransformSpace == EditorApp::EditorTransformSpace::Local ? "Local" : "World"))
        {
            if (ImGui::Selectable("Local", sceneState.TransformSpace == EditorApp::EditorTransformSpace::Local))
                sceneState.TransformSpace = EditorApp::EditorTransformSpace::Local;
            if (ImGui::Selectable("World", sceneState.TransformSpace == EditorApp::EditorTransformSpace::World))
                sceneState.TransformSpace = EditorApp::EditorTransformSpace::World;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &sceneState.SnapEnabled);
        if (sceneState.SnapEnabled)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::DragFloat("##TranslationSnap", &sceneState.TranslationSnap, 0.01f, 0.001f, 100.0f, "T %.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::DragFloat("##RotationSnap", &sceneState.RotationSnapDegrees, 0.25f, 0.1f, 180.0f, "R %.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::DragFloat("##ScaleSnap", &sceneState.ScaleSnap, 0.01f, 0.001f, 10.0f, "S %.2f");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", ResolveViewportToolLabel(sceneState.ViewportTool));
    }

    bool ProjectWorldToScreen(const Life::Camera& camera,
                              const glm::vec3& worldPosition,
                              const ImVec2& viewportTopLeft,
                              const ImVec2& viewportSize,
                              ImVec2& screenPosition)
    {
        const glm::vec4 clip = camera.GetViewProjectionMatrix() * glm::vec4(worldPosition, 1.0f);
        if (std::abs(clip.w) <= std::numeric_limits<float>::epsilon())
            return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        screenPosition.x = viewportTopLeft.x + ((ndc.x + 1.0f) * 0.5f * viewportSize.x);
        screenPosition.y = viewportTopLeft.y + ((ndc.y + 1.0f) * 0.5f * viewportSize.y);
        return std::isfinite(screenPosition.x) && std::isfinite(screenPosition.y);
    }

    bool ContainsPoint(const ImVec2& minimum, const ImVec2& maximum, const ImVec2& point) noexcept
    {
        return point.x >= minimum.x && point.x <= maximum.x && point.y >= minimum.y && point.y <= maximum.y;
    }

    bool TransformChanged(const Life::TransformComponent& left, const Life::TransformComponent& right) noexcept
    {
        constexpr float epsilon = 0.0001f;
        return glm::length(left.LocalPosition - right.LocalPosition) > epsilon ||
            glm::length(left.LocalRotation - right.LocalRotation) > epsilon ||
            glm::length(left.LocalScale - right.LocalScale) > epsilon;
    }

    Life::Entity PickSpriteEntity(const Life::Scene& scene,
                                  const Life::Camera& camera,
                                  const ImVec2& viewportTopLeft,
                                  const ImVec2& viewportSize,
                                  const ImVec2& mousePosition)
    {
        Life::Entity bestEntity;
        float bestDepth = -std::numeric_limits<float>::infinity();

        for (const Life::Entity entity : scene.GetEntities())
        {
            if (!entity.IsEnabled())
                continue;

            const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
            if (sprite == nullptr)
                continue;

            const glm::mat4 worldTransform = scene.GetWorldTransformMatrix(entity);
            const glm::vec3 center = glm::vec3(worldTransform[3]);
            const glm::vec3 xAxis = glm::vec3(worldTransform * glm::vec4(sprite->Size.x, 0.0f, 0.0f, 0.0f));
            const glm::vec3 yAxis = glm::vec3(worldTransform * glm::vec4(0.0f, sprite->Size.y, 0.0f, 0.0f));
            const std::array<glm::vec3, 4> corners{{
                center - (xAxis * 0.5f) - (yAxis * 0.5f),
                center + (xAxis * 0.5f) - (yAxis * 0.5f),
                center + (xAxis * 0.5f) + (yAxis * 0.5f),
                center - (xAxis * 0.5f) + (yAxis * 0.5f)
            }};

            ImVec2 minPoint(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
            ImVec2 maxPoint(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
            bool anyProjected = false;
            for (const glm::vec3& corner : corners)
            {
                ImVec2 projected;
                if (!ProjectWorldToScreen(camera, corner, viewportTopLeft, viewportSize, projected))
                    continue;

                minPoint.x = std::min(minPoint.x, projected.x);
                minPoint.y = std::min(minPoint.y, projected.y);
                maxPoint.x = std::max(maxPoint.x, projected.x);
                maxPoint.y = std::max(maxPoint.y, projected.y);
                anyProjected = true;
            }

            if (!anyProjected || !ContainsPoint(minPoint, maxPoint, mousePosition))
                continue;

            const float depth = (camera.GetViewMatrix() * glm::vec4(center, 1.0f)).z;
            if (!bestEntity.IsValid() || depth > bestDepth)
            {
                bestEntity = entity;
                bestDepth = depth;
            }
        }

        return bestEntity;
    }

#if __has_include(<ImGuizmo.h>)
    ImGuizmo::OPERATION ResolveGizmoOperation(EditorApp::EditorViewportTool tool) noexcept
    {
        switch (tool)
        {
            case EditorApp::EditorViewportTool::Rotate: return ImGuizmo::ROTATE;
            case EditorApp::EditorViewportTool::Scale: return ImGuizmo::SCALE;
            case EditorApp::EditorViewportTool::Translate: return ImGuizmo::TRANSLATE;
            case EditorApp::EditorViewportTool::Select:
            default: return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE ResolveGizmoMode(EditorApp::EditorTransformSpace space) noexcept
    {
        return space == EditorApp::EditorTransformSpace::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    std::array<float, 3> ResolveSnapValues(const EditorApp::EditorSceneState& sceneState)
    {
        switch (sceneState.ViewportTool)
        {
            case EditorApp::EditorViewportTool::Rotate:
                return { sceneState.RotationSnapDegrees, sceneState.RotationSnapDegrees, sceneState.RotationSnapDegrees };
            case EditorApp::EditorViewportTool::Scale:
                return { sceneState.ScaleSnap, sceneState.ScaleSnap, sceneState.ScaleSnap };
            case EditorApp::EditorViewportTool::Translate:
            case EditorApp::EditorViewportTool::Select:
            default:
                return { sceneState.TranslationSnap, sceneState.TranslationSnap, sceneState.TranslationSnap };
        }
    }
#endif
#endif
}

namespace EditorApp
{
    void SceneViewportPanel::Attach(const EditorServices& services)
    {
        m_NativeWindowHandle = services.Window ? services.Window->get().GetNativeHandle() : nullptr;

        if (services.Renderer && services.SceneRenderer2D && services.ImGuiSystem)
        {
            m_SceneSurface = Life::CreateScope<Life::SceneSurface>(
                services.Renderer->get(),
                services.SceneRenderer2D->get().GetRenderer2D(),
                services.ImGuiSystem->get());
        }
    }

    void SceneViewportPanel::Detach() noexcept
    {
        SetCameraNavigationActive(false);
        m_SceneSurface.reset();
        m_State = {};
        m_LastTimestep = 0.0f;
        m_NativeWindowHandle = nullptr;
    }

    void SceneViewportPanel::Update(const EditorServices& services, float timestep)
    {
        m_LastTimestep = timestep;
        if (m_NativeWindowHandle == nullptr && services.Window)
            m_NativeWindowHandle = services.Window->get().GetNativeHandle();
    }

    void SceneViewportPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool, EditorUndoStack& undoStack)
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
        {
            SetCameraNavigationActive(false);
            return;
        }

        if (ImGui::Begin("Scene", &isOpen))
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Scene");
            ImGui::SameLine();
            ImGui::TextDisabled("Viewport and scene interaction");
            ImGui::Separator();
            DrawViewportToolbar(sceneState);
            ImGui::Separator();

            const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
            if (availableRegion.x >= 1.0f && availableRegion.y >= 1.0f)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                if (ImGui::BeginChild(
                        "SceneViewportSurface",
                        availableRegion,
                        false,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                {
                    const ImVec2 viewportTopLeft = ImGui::GetCursorScreenPos();
                    const ImVec2 viewportRegion = ImGui::GetContentRegionAvail();
                    const ImVec2 framebufferScale = ResolveFramebufferScale(services, m_NativeWindowHandle);
                    const float framebufferScaleX = std::max(framebufferScale.x, 1.0f);
                    const float framebufferScaleY = std::max(framebufferScale.y, 1.0f);
                    const ImVec2 snappedViewportTopLeft = {
                        SnapToPixelGrid(viewportTopLeft.x, framebufferScaleX),
                        SnapToPixelGrid(viewportTopLeft.y, framebufferScaleY)
                    };
                    const uint32_t renderWidth = std::max(1u, static_cast<uint32_t>(std::lround(viewportRegion.x * framebufferScaleX)));
                    const uint32_t renderHeight = std::max(1u, static_cast<uint32_t>(std::lround(viewportRegion.y * framebufferScaleY)));
                    const ImVec2 snappedViewportRegion = {
                        static_cast<float>(renderWidth) / framebufferScaleX,
                        static_cast<float>(renderHeight) / framebufferScaleY
                    };
                    const ImVec2 mainViewportPosition = ImGui::GetWindowViewport()->Pos;
                    const ImVec2 viewportWindowLocalTopLeft = {
                        snappedViewportTopLeft.x - mainViewportPosition.x,
                        snappedViewportTopLeft.y - mainViewportPosition.y
                    };
                    const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                    m_State.DisplayWidth = snappedViewportRegion.x;
                    m_State.DisplayHeight = snappedViewportRegion.y;
                    m_State.FramebufferScaleX = framebufferScaleX;
                    m_State.FramebufferScaleY = framebufferScaleY;
                    m_State.RequestedRenderWidth = renderWidth;
                    m_State.RequestedRenderHeight = renderHeight;
                    if (services.GraphicsDevice)
                    {
                        m_State.BackBufferWidth = services.GraphicsDevice->get().GetBackBufferWidth();
                        m_State.BackBufferHeight = services.GraphicsDevice->get().GetBackBufferHeight();
                    }
                    else
                    {
                        m_State.BackBufferWidth = 0;
                        m_State.BackBufferHeight = 0;
                    }
                    if (renderWidth >= 1u && renderHeight >= 1u)
                    {
                        ImGui::SetCursorScreenPos(snappedViewportTopLeft);
                        const bool rendered = RenderSceneSurface(
                            renderWidth,
                            renderHeight,
                            snappedViewportRegion.x,
                            snappedViewportRegion.y,
                            snappedViewportTopLeft.x,
                            snappedViewportTopLeft.y,
                            services,
                            sceneState,
                            cameraTool,
                            undoStack,
                            viewportHovered,
                            viewportFocused);
                        if (m_CameraNavigationActive)
                            SetCameraNavigationMouseRect(m_NativeWindowHandle, viewportWindowLocalTopLeft, snappedViewportRegion);

                        if (!rendered)
                        {
                            SetCameraNavigationActive(false);
                            ImGui::TextUnformatted("Scene surface rendering is unavailable.");
                        }
                        else if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                            {
                                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                                if (assetPayload != nullptr &&
                                    assetPayload->Kind == ProjectAssetPayloadKind::Scene &&
                                    assetPayload->RelativePath[0] != '\0' &&
                                    services.SceneService)
                                {
                                    sceneState.ResetRuntimeState();
                                    const std::string sceneAssetKey = std::string("Assets/") + assetPayload->RelativePath.data();
                                    const auto loadResult = services.SceneService->get().LoadScene(sceneAssetKey);
                                    if (loadResult.IsFailure())
                                    {
                                        sceneState.SetStatusMessage(loadResult.GetError().GetErrorMessage(), true);
                                    }
                                    else
                                    {
                                        sceneState.ClearSelection();
                                        sceneState.SetStatusMessage(
                                            "Opened scene '" + services.SceneService->get().GetActiveScene().GetName() + "'.",
                                            false);
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    else
                    {
                        SetCameraNavigationActive(false);
                        ImGui::TextUnformatted("Scene surface has no drawable area.");
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
            else
            {
                SetCameraNavigationActive(false);
                ImGui::TextUnformatted("Scene surface has no drawable area.");
            }
        }
        else
        {
            SetCameraNavigationActive(false);
        }
        ImGui::End();
#else
        (void)isOpen;
        (void)services;
        (void)sceneState;
        (void)cameraTool;
        (void)undoStack;
#endif
    }

    const SceneViewportState& SceneViewportPanel::GetState() const noexcept
    {
        return m_State;
    }

    void SceneViewportPanel::UpdateCameraNavigation(EditorCameraTool& cameraTool, Life::Camera& camera, bool viewportHovered, bool viewportFocused)
    {
#if __has_include(<imgui.h>)
        const bool activateNavigation = viewportFocused && ImGui::IsMouseDown(ImGuiMouseButton_Right) && (viewportHovered || m_CameraNavigationActive);
        SetCameraNavigationActive(activateNavigation);
        if (!m_CameraNavigationActive)
            return;

        EditorCameraTool::FlyCameraInput input;
        input.LookDelta = GetCameraLookDelta(m_NativeWindowHandle);
        input.MoveAxes.x = (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0.0f);
        input.MoveAxes.y = (ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f);
        input.MoveAxes.z = (ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0.0f);
        input.Boost = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        cameraTool.UpdateFlyCamera(camera, input, m_LastTimestep);
#else
        (void)cameraTool;
        (void)camera;
        (void)viewportHovered;
        (void)viewportFocused;
#endif
    }

    void SceneViewportPanel::SetCameraNavigationActive(bool active) noexcept
    {
        if (m_CameraNavigationActive == active)
            return;

        m_CameraNavigationActive = active;

        if (auto* sdlWindow = static_cast<SDL_Window*>(m_NativeWindowHandle))
        {
            SDL_SetWindowMouseGrab(sdlWindow, active);
            SDL_CaptureMouse(active);
            SDL_SetWindowRelativeMouseMode(sdlWindow, active);
            if (!active)
                SDL_SetWindowMouseRect(sdlWindow, nullptr);

            if (active && SDL_GetWindowRelativeMouseMode(sdlWindow))
            {
                float deltaX = 0.0f;
                float deltaY = 0.0f;
                SDL_GetRelativeMouseState(&deltaX, &deltaY);
            }
        }
    }

    bool SceneViewportPanel::RenderSceneSurface(uint32_t renderWidth,
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
                                                bool viewportFocused)
    {
        m_State.LastRenderSucceeded = false;
        m_State.ExecutionMode = sceneState.ExecutionMode;
        m_State.UsingEditorCamera = sceneState.ExecutionMode == EditorSceneExecutionMode::Edit;
        m_State.UsingSceneCamera = sceneState.ExecutionMode != EditorSceneExecutionMode::Edit;
        m_State.SurfaceReady = m_SceneSurface && m_SceneSurface->IsReady();
        m_State.SurfaceWidth = m_SceneSurface ? m_SceneSurface->GetWidth() : 0;
        m_State.SurfaceHeight = m_SceneSurface ? m_SceneSurface->GetHeight() : 0;
        m_State.RequestedRenderWidth = renderWidth;
        m_State.RequestedRenderHeight = renderHeight;
        m_State.DisplayWidth = displayWidth;
        m_State.DisplayHeight = displayHeight;
        m_State.BackBufferWidth = services.GraphicsDevice ? services.GraphicsDevice->get().GetBackBufferWidth() : 0;
        m_State.BackBufferHeight = services.GraphicsDevice ? services.GraphicsDevice->get().GetBackBufferHeight() : 0;
        m_State.RequestedQuadCount = 0;
        m_State.TexturedQuadCount = 0;
        m_State.UntexturedQuadCount = 0;
        m_State.RendererStats = {};

        if (!m_SceneSurface || !services.SceneRenderer2D || !services.CameraManager)
        {
            SetCameraNavigationActive(false);
            return false;
        }

        if (!m_SceneSurface->Resize(renderWidth, renderHeight))
        {
            SetCameraNavigationActive(false);
            return false;
        }

        const float requestedAspectRatio = displayHeight > 0.0f ? displayWidth / displayHeight : 16.0f / 9.0f;
        const float actualAspectRatio = m_SceneSurface->GetHeight() > 0
            ? static_cast<float>(m_SceneSurface->GetWidth()) / static_cast<float>(m_SceneSurface->GetHeight())
            : requestedAspectRatio;

        Life::Scene* effectiveScene = services.SceneService ? sceneState.GetEffectiveScene(services.SceneService->get()) : nullptr;
        Life::Camera sceneCamera;
        Life::Camera* activeCamera = nullptr;
        const bool useSceneCamera = sceneState.ExecutionMode != EditorSceneExecutionMode::Edit;
        if (useSceneCamera)
        {
            SetCameraNavigationActive(false);
            if (effectiveScene == nullptr || !effectiveScene->BuildPrimaryCamera(actualAspectRatio, sceneCamera))
                return false;
            activeCamera = &sceneCamera;
        }
        else
        {
            cameraTool.Ensure(services.CameraManager->get(), requestedAspectRatio);
            auto editorCamera = cameraTool.TryGetCamera(services.CameraManager->get());
            if (!editorCamera)
            {
                SetCameraNavigationActive(false);
                return false;
            }

            Life::Camera& camera = editorCamera->get();
            camera.SetAspectRatio(actualAspectRatio);
            UpdateCameraNavigation(cameraTool, camera, viewportHovered, viewportFocused);
            activeCamera = &camera;
        }

        m_State.UsingEditorCamera = !useSceneCamera;
        m_State.UsingSceneCamera = useSceneCamera;

        bool renderSucceeded = false;
        if (effectiveScene != nullptr)
        {
            m_State.RequestedQuadCount = 0;
            m_State.TexturedQuadCount = 0;
            m_State.UntexturedQuadCount = 0;

            for (const Life::Entity entity : effectiveScene->GetEntities())
            {
                if (const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>())
                {
                    ++m_State.RequestedQuadCount;
                    if (sprite->TextureAsset)
                        ++m_State.TexturedQuadCount;
                    else
                        ++m_State.UntexturedQuadCount;
                }
            }

            renderSucceeded = services.SceneRenderer2D->get().RenderToSurface(
                *m_SceneSurface,
                *effectiveScene,
                *activeCamera);
        }
        else
        {
            const Life::SceneRenderer2D::Scene2D emptyScene{ .Camera = activeCamera };
            renderSucceeded = services.SceneRenderer2D->get().RenderToSurface(*m_SceneSurface, emptyScene);
        }

        if (!renderSucceeded)
            return false;

        m_State.SurfaceReady = m_SceneSurface->IsReady();
        m_State.SurfaceWidth = m_SceneSurface->GetWidth();
        m_State.SurfaceHeight = m_SceneSurface->GetHeight();
        m_State.RendererStats = services.SceneRenderer2D->get().GetStats();

        const bool presented = m_SceneSurface->Present(displayWidth, displayHeight);
        m_State.LastRenderSucceeded = presented;
#if __has_include(<imgui.h>)
        if (presented &&
            !useSceneCamera &&
            effectiveScene != nullptr &&
            services.SceneService)
        {
            const ImVec2 viewportTopLeft(viewportScreenX, viewportScreenY);
            const ImVec2 viewportSize(displayWidth, displayHeight);
            const ImVec2 mousePosition = ImGui::GetIO().MousePos;

#if __has_include(<ImGuizmo.h>)
            ImGuizmo::BeginFrame();
            ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);
            ImGuizmo::SetOrthographic(activeCamera->GetProjectionType() == Life::ProjectionType::Orthographic);

            Life::Entity selectedEntity = sceneState.GetSelectedEntity(*effectiveScene);
            const bool canShowGizmo = selectedEntity.IsValid() &&
                sceneState.ViewportTool != EditorViewportTool::Select &&
                selectedEntity.HasComponent<Life::TransformComponent>();

            if (canShowGizmo)
            {
                glm::mat4 worldTransform = effectiveScene->GetWorldTransformMatrix(selectedEntity);
                const glm::mat4 viewMatrix = activeCamera->GetViewMatrix();
                const glm::mat4 projectionMatrix = activeCamera->GetProjectionMatrix();
                std::array<float, 3> snapValues = ResolveSnapValues(sceneState);
                const float* snap = sceneState.SnapEnabled ? snapValues.data() : nullptr;

                const bool manipulated = ImGuizmo::Manipulate(
                    glm::value_ptr(viewMatrix),
                    glm::value_ptr(projectionMatrix),
                    ResolveGizmoOperation(sceneState.ViewportTool),
                    ResolveGizmoMode(sceneState.TransformSpace),
                    glm::value_ptr(worldTransform),
                    nullptr,
                    snap);

                const bool usingGizmo = ImGuizmo::IsUsing();
                if (usingGizmo && (!m_GizmoManipulating || m_GizmoEntityId != selectedEntity.GetId()))
                {
                    m_GizmoManipulating = true;
                    m_GizmoEntityId = selectedEntity.GetId();
                    m_GizmoTransformBefore = selectedEntity.GetComponent<Life::TransformComponent>();
                }

                if (manipulated)
                    (void)Life::SetEntityWorldTransform(*effectiveScene, selectedEntity, worldTransform);

                if (!usingGizmo && m_GizmoManipulating)
                {
                    Life::Entity manipulatedEntity = effectiveScene->FindEntityById(m_GizmoEntityId);
                    if (manipulatedEntity.IsValid())
                    {
                        const Life::TransformComponent after = manipulatedEntity.GetComponent<Life::TransformComponent>();
                        if (TransformChanged(m_GizmoTransformBefore, after))
                        {
                            undoStack.CommitExecuted(std::make_unique<SetEntityTransformCommand>(
                                m_GizmoEntityId,
                                m_GizmoTransformBefore,
                                after));
                            services.SceneService->get().MarkActiveSceneDirty();
                        }
                    }

                    m_GizmoManipulating = false;
                    m_GizmoEntityId.clear();
                    m_GizmoTransformBefore = {};
                }
            }
            else if (m_GizmoManipulating && !ImGuizmo::IsUsing())
            {
                m_GizmoManipulating = false;
                m_GizmoEntityId.clear();
                m_GizmoTransformBefore = {};
            }

            const bool gizmoOwnsMouse = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
#else
            const bool gizmoOwnsMouse = false;
            (void)undoStack;
#endif
            if (viewportHovered &&
                viewportFocused &&
                !m_CameraNavigationActive &&
                !gizmoOwnsMouse &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                Life::Entity pickedEntity = PickSpriteEntity(*effectiveScene, *activeCamera, viewportTopLeft, viewportSize, mousePosition);
                if (pickedEntity.IsValid())
                    sceneState.SelectEntity(pickedEntity);
                else
                    sceneState.ClearSelection();
            }
        }
        else
        {
            m_GizmoManipulating = false;
            m_GizmoEntityId.clear();
            m_GizmoTransformBefore = {};
        }
#else
        (void)viewportScreenX;
        (void)viewportScreenY;
        (void)undoStack;
#endif
        return presented;
    }
}
