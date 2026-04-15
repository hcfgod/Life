#include "Editor/Viewport/SceneViewportPanel.h"

#include "Editor/Camera/EditorCameraTool.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#include <SDL3/SDL.h>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace
{
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

    void SceneViewportPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool)
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
                    const ImVec2 mainViewportPosition = ImGui::GetMainViewport()->Pos;
                    const ImVec2 viewportWindowLocalTopLeft = {
                        viewportTopLeft.x - mainViewportPosition.x,
                        viewportTopLeft.y - mainViewportPosition.y
                    };
                    const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                    if (viewportRegion.x >= 1.0f && viewportRegion.y >= 1.0f)
                    {
                        const bool rendered = RenderSceneSurface(
                            static_cast<uint32_t>(viewportRegion.x),
                            static_cast<uint32_t>(viewportRegion.y),
                            services,
                            sceneState,
                            cameraTool,
                            viewportHovered,
                            viewportFocused);
                        if (m_CameraNavigationActive)
                            SetCameraNavigationMouseRect(m_NativeWindowHandle, viewportWindowLocalTopLeft, viewportRegion);

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

    bool SceneViewportPanel::RenderSceneSurface(uint32_t width, uint32_t height, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool, bool viewportHovered, bool viewportFocused)
    {
        m_State.LastRenderSucceeded = false;
        m_State.ExecutionMode = sceneState.ExecutionMode;
        m_State.UsingEditorCamera = sceneState.ExecutionMode == EditorSceneExecutionMode::Edit;
        m_State.UsingSceneCamera = sceneState.ExecutionMode != EditorSceneExecutionMode::Edit;
        m_State.SurfaceReady = m_SceneSurface && m_SceneSurface->IsReady();
        m_State.SurfaceWidth = m_SceneSurface ? m_SceneSurface->GetWidth() : 0;
        m_State.SurfaceHeight = m_SceneSurface ? m_SceneSurface->GetHeight() : 0;
        m_State.RequestedQuadCount = 0;
        m_State.TexturedQuadCount = 0;
        m_State.UntexturedQuadCount = 0;
        m_State.RendererStats = {};

        if (!m_SceneSurface || !services.SceneRenderer2D || !services.CameraManager)
        {
            SetCameraNavigationActive(false);
            return false;
        }

        if (!m_SceneSurface->Resize(width, height))
        {
            SetCameraNavigationActive(false);
            return false;
        }

        const float requestedAspectRatio = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
        const float actualAspectRatio = m_SceneSurface->GetHeight() > 0
            ? static_cast<float>(m_SceneSurface->GetWidth()) / static_cast<float>(m_SceneSurface->GetHeight())
            : requestedAspectRatio;

        const Life::Scene* effectiveScene = services.SceneService ? sceneState.GetEffectiveScene(services.SceneService->get()) : nullptr;
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

        const bool presented = m_SceneSurface->Present(static_cast<float>(width), static_cast<float>(height));
        m_State.LastRenderSucceeded = presented;
        return presented;
    }
}
