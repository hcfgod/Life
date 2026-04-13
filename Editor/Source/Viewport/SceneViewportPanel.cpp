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
    constexpr uint32_t MaxStressGridDimension = 512;
}

namespace EditorApp
{
    SceneViewportPanel::SceneViewportPanel(std::string checkerTextureKey)
        : m_CheckerTextureKey(std::move(checkerTextureKey))
    {
    }

    void SceneViewportPanel::Attach(const EditorServices& services)
    {
        TryAcquireCheckerTexture(services);
        m_NativeWindowHandle = services.Window ? services.Window->get().GetNativeHandle() : nullptr;

        if (services.Renderer && services.SceneRenderer2D && services.ImGuiSystem)
        {
            m_SceneSurface = Life::CreateScope<Life::SceneSurface>(
                services.Renderer->get(),
                services.SceneRenderer2D->get().GetRenderer2D(),
                services.ImGuiSystem->get());
        }

        if (!m_CheckerTextureAsset)
            LOG_WARN("Editor failed to load textured quad asset '{}'. Falling back to error texture.", m_CheckerTextureKey);
    }

    void SceneViewportPanel::Detach() noexcept
    {
        SetCameraNavigationActive(false);
        m_SceneSurface.reset();
        m_CheckerTextureAsset.reset();
        m_State = {};
        m_ElapsedTime = 0.0f;
        m_LastTimestep = 0.0f;
        m_NativeWindowHandle = nullptr;
    }

    void SceneViewportPanel::Update(const EditorServices& services, float timestep)
    {
        m_ElapsedTime += timestep;
        m_LastTimestep = timestep;
        if (m_NativeWindowHandle == nullptr && services.Window)
            m_NativeWindowHandle = services.Window->get().GetNativeHandle();

        TryAcquireCheckerTexture(services);
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
                    const ImVec2 viewportRegion = ImGui::GetContentRegionAvail();
                    const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                    if (viewportRegion.x >= 1.0f && viewportRegion.y >= 1.0f)
                    {
                        const bool rendered = RenderSceneSurface(
                            static_cast<uint32_t>(viewportRegion.x),
                            static_cast<uint32_t>(viewportRegion.y),
                            services,
                            cameraTool,
                            viewportHovered,
                            viewportFocused);
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

    void SceneViewportPanel::RenderStressPanel(bool& isOpen)
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Renderer Stress", &isOpen))
            RenderStressControls();

        ImGui::End();
#else
        (void)isOpen;
#endif
    }

    const SceneViewportState& SceneViewportPanel::GetState() const noexcept
    {
        return m_State;
    }

    void SceneViewportPanel::RenderStressControls()
    {
#if __has_include(<imgui.h>)
        if (!ImGui::CollapsingHeader("Renderer Stress Test", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        ImGui::Checkbox("Enable", &m_StressSettings.Enabled);

        int columns = static_cast<int>(m_StressSettings.Columns);
        int rows = static_cast<int>(m_StressSettings.Rows);
        if (ImGui::SliderInt("Columns", &columns, 1, static_cast<int>(MaxStressGridDimension)))
            m_StressSettings.Columns = static_cast<uint32_t>(columns);

        if (ImGui::SliderInt("Rows", &rows, 1, static_cast<int>(MaxStressGridDimension)))
            m_StressSettings.Rows = static_cast<uint32_t>(rows);

        float quadSize[2] = { m_StressSettings.QuadSize.x, m_StressSettings.QuadSize.y };
        if (ImGui::SliderFloat2("Quad Size", quadSize, 0.05f, 3.0f, "%.2f"))
            m_StressSettings.QuadSize = { quadSize[0], quadSize[1] };

        ImGui::SliderFloat("Spacing", &m_StressSettings.Spacing, 0.05f, 3.0f, "%.2f");
        ImGui::Checkbox("Draw Textured Quads", &m_StressSettings.DrawTexturedQuads);
        ImGui::SameLine();
        ImGui::Checkbox("Draw Colored Quads", &m_StressSettings.DrawColoredQuads);
        ImGui::SliderFloat("Textured Mix", &m_StressSettings.TexturedMix, 0.0f, 1.0f, "%.2f");
        ImGui::Checkbox("Animate", &m_StressSettings.Animate);
        ImGui::SliderFloat("Motion Amplitude", &m_StressSettings.MotionAmplitude, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Rotation Speed", &m_StressSettings.RotationSpeed, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Depth Step", &m_StressSettings.DepthStep, 0.0f, 0.01f, "%.4f");

        const uint32_t configuredQuadCount = GetConfiguredQuadCount();
        ImGui::Text("Configured Quads: %u", configuredQuadCount);
        ImGui::Text("Checker Texture: %s", m_CheckerTextureAsset ? "Loaded" : "Unavailable");

        if (ImGui::Button("Reset Stress Settings"))
            m_StressSettings = {};
#endif
    }

    void SceneViewportPanel::UpdateCameraNavigation(EditorCameraTool& cameraTool, Life::Camera& camera, bool viewportHovered, bool viewportFocused)
    {
#if __has_include(<imgui.h>)
        const bool activateNavigation = viewportHovered && viewportFocused && ImGui::IsMouseDown(ImGuiMouseButton_Right);
        SetCameraNavigationActive(activateNavigation);
        if (!m_CameraNavigationActive)
            return;

        EditorCameraTool::FlyCameraInput input;
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        input.LookDelta = { mouseDelta.x, mouseDelta.y };
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
            SDL_SetWindowRelativeMouseMode(sdlWindow, active);
    }

    uint32_t SceneViewportPanel::GetConfiguredQuadCount() const noexcept
    {
        const bool isEnabled = m_StressSettings.Enabled && (m_StressSettings.DrawTexturedQuads || m_StressSettings.DrawColoredQuads);
        if (!isEnabled)
            return 0;

        const uint32_t columns = std::clamp(m_StressSettings.Columns, 1u, MaxStressGridDimension);
        const uint32_t rows = std::clamp(m_StressSettings.Rows, 1u, MaxStressGridDimension);
        const uint64_t requestedQuadCount = static_cast<uint64_t>(columns) * static_cast<uint64_t>(rows);

        return static_cast<uint32_t>(requestedQuadCount);
    }

    void SceneViewportPanel::TryAcquireCheckerTexture(const EditorServices& services)
    {
        if (m_CheckerTextureAsset || !services.AssetManager)
            return;

        m_CheckerTextureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(m_CheckerTextureKey);
        if (!m_CheckerTextureAsset)
            return;

        m_CheckerTextureAsset->SetFilterModes(Life::TextureFilterMode::Nearest, Life::TextureFilterMode::Nearest);
        m_CheckerTextureAsset->SetWrapModes(Life::TextureWrapMode::Repeat, Life::TextureWrapMode::Repeat);
        LOG_INFO("Editor recovered textured quad asset '{}'.", m_CheckerTextureKey);
    }

    Life::SceneRenderer2D::Scene2D SceneViewportPanel::BuildScene2D(const Life::Camera& camera)
    {
        Life::SceneRenderer2D::Scene2D scene;
        scene.Camera = &camera;

        m_State.RequestedQuadCount = 0;
        m_State.TexturedQuadCount = 0;
        m_State.UntexturedQuadCount = 0;

        const uint32_t configuredQuadCount = GetConfiguredQuadCount();
        if (configuredQuadCount == 0)
            return scene;

        const uint32_t columns = std::clamp(m_StressSettings.Columns, 1u, MaxStressGridDimension);
        const uint32_t rows = std::clamp(m_StressSettings.Rows, 1u, MaxStressGridDimension);
        const glm::vec2 quadSize = {
            std::max(m_StressSettings.QuadSize.x, 0.05f),
            std::max(m_StressSettings.QuadSize.y, 0.05f)
        };
        const float spacing = std::max(m_StressSettings.Spacing, 0.05f);
        const float motionAmplitude = std::max(m_StressSettings.MotionAmplitude, 0.0f);
        const float rotationSpeed = std::max(m_StressSettings.RotationSpeed, 0.0f);
        const float depthStep = std::max(m_StressSettings.DepthStep, 0.0f);
        const float texturedMix = std::clamp(m_StressSettings.TexturedMix, 0.0f, 1.0f);
        const uint32_t texturedQuadTarget = m_StressSettings.DrawTexturedQuads
            ? (m_StressSettings.DrawColoredQuads
                ? std::min(configuredQuadCount, static_cast<uint32_t>(std::round(texturedMix * static_cast<float>(configuredQuadCount))))
                : configuredQuadCount)
            : 0u;
        const float halfGridWidth = static_cast<float>(columns - 1u) * spacing * 0.5f;
        const float halfGridHeight = static_cast<float>(rows - 1u) * spacing * 0.5f;

        scene.Quads.reserve(configuredQuadCount);

        uint32_t quadIndex = 0;
        for (uint32_t row = 0; row < rows && quadIndex < configuredQuadCount; ++row)
        {
            for (uint32_t column = 0; column < columns && quadIndex < configuredQuadCount; ++column)
            {
                const float phase = static_cast<float>(quadIndex) * 0.1375f;
                const float offsetX = m_StressSettings.Animate ? std::sin(m_ElapsedTime * 1.35f + phase) * motionAmplitude : 0.0f;
                const float offsetY = m_StressSettings.Animate ? std::cos(m_ElapsedTime * 0.9f + phase * 1.17f) * motionAmplitude : 0.0f;
                const float positionX = static_cast<float>(column) * spacing - halfGridWidth + offsetX;
                const float positionY = halfGridHeight - static_cast<float>(row) * spacing + offsetY;
                const bool wantsTexturedQuad = m_StressSettings.DrawTexturedQuads && quadIndex < texturedQuadTarget;
                const bool useTexture = wantsTexturedQuad && static_cast<bool>(m_CheckerTextureAsset);

                Life::SceneRenderer2D::QuadCommand quad;
                quad.Position = { positionX, positionY, -static_cast<float>(quadIndex) * depthStep };
                quad.Size = quadSize;
                quad.RotationRadians = m_StressSettings.Animate ? (m_ElapsedTime * rotationSpeed + phase * 0.2f) : 0.0f;

                if (useTexture)
                {
                    const float tint = 0.85f + 0.15f * std::sin(phase * 0.8f);
                    quad.Color = { tint, tint, tint, 1.0f };
                    quad.TextureAsset = m_CheckerTextureAsset.get();
                    ++m_State.TexturedQuadCount;
                }
                else
                {
                    const float red = 0.25f + 0.65f * (static_cast<float>((column % 7u) + 1u) / 7.0f);
                    const float green = 0.25f + 0.65f * (static_cast<float>((row % 9u) + 1u) / 9.0f);
                    const float blue = 0.35f + 0.45f * (0.5f + 0.5f * std::sin(phase * 0.65f));
                    quad.Color = wantsTexturedQuad
                        ? glm::vec4{ 1.0f, 0.0f, 1.0f, 1.0f }
                        : glm::vec4{ red, green, blue, 0.92f };
                    ++m_State.UntexturedQuadCount;
                }

                scene.Quads.push_back(quad);
                ++quadIndex;
            }
        }

        m_State.RequestedQuadCount = static_cast<uint32_t>(scene.Quads.size());

        return scene;
    }

    bool SceneViewportPanel::RenderSceneSurface(uint32_t width, uint32_t height, const EditorServices& services, EditorCameraTool& cameraTool, bool viewportHovered, bool viewportFocused)
    {
        m_State.LastRenderSucceeded = false;
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

        const float requestedAspectRatio = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
        cameraTool.Ensure(services.CameraManager->get(), requestedAspectRatio);

        auto editorCamera = cameraTool.TryGetCamera(services.CameraManager->get());
        if (!editorCamera)
        {
            SetCameraNavigationActive(false);
            return false;
        }

        Life::Camera& camera = editorCamera->get();

        if (!m_SceneSurface->Resize(width, height))
        {
            SetCameraNavigationActive(false);
            return false;
        }

        const float actualAspectRatio = m_SceneSurface->GetHeight() > 0
            ? static_cast<float>(m_SceneSurface->GetWidth()) / static_cast<float>(m_SceneSurface->GetHeight())
            : requestedAspectRatio;
        camera.SetAspectRatio(actualAspectRatio);
        UpdateCameraNavigation(cameraTool, camera, viewportHovered, viewportFocused);

        bool renderSucceeded = false;
        if (services.SceneService && services.SceneService->get().HasActiveScene())
        {
            m_State.RequestedQuadCount = static_cast<uint32_t>(services.SceneService->get().GetActiveScene().GetEntityCount());
            m_State.TexturedQuadCount = 0;
            m_State.UntexturedQuadCount = 0;

            for (const Life::Entity entity : services.SceneService->get().GetActiveScene().GetEntities())
            {
                if (const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>())
                {
                    if (sprite->TextureAsset)
                        ++m_State.TexturedQuadCount;
                    else
                        ++m_State.UntexturedQuadCount;
                }
            }

            renderSucceeded = services.SceneRenderer2D->get().RenderToSurface(
                *m_SceneSurface,
                services.SceneService->get().GetActiveScene(),
                camera);
        }
        else
        {
            Life::SceneRenderer2D::Scene2D scene = BuildScene2D(camera);
            renderSucceeded = services.SceneRenderer2D->get().RenderToSurface(*m_SceneSurface, scene);
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
