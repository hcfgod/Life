#include "Editor/Viewport/SceneViewportPanel.h"

#include "Editor/Camera/EditorCameraTool.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include <cmath>

namespace EditorApp
{
    SceneViewportPanel::SceneViewportPanel(std::string checkerTextureKey)
        : m_CheckerTextureKey(std::move(checkerTextureKey))
    {
    }

    void SceneViewportPanel::Attach(const EditorServices& services)
    {
        TryAcquireCheckerTexture(services);

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
        m_SceneSurface.reset();
        m_CheckerTextureAsset.reset();
        m_State = {};
        m_ElapsedTime = 0.0f;
    }

    void SceneViewportPanel::Update(const EditorServices& services, float timestep)
    {
        m_ElapsedTime += timestep;
        TryAcquireCheckerTexture(services);
    }

    void SceneViewportPanel::Render(bool& isOpen, const EditorServices& services, EditorCameraTool& cameraTool)
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::Begin("Scene", &isOpen))
        {
            const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
            if (availableRegion.x >= 1.0f && availableRegion.y >= 1.0f)
            {
                if (!RenderSceneSurface(
                        static_cast<uint32_t>(availableRegion.x),
                        static_cast<uint32_t>(availableRegion.y),
                        services,
                        cameraTool))
                {
                    ImGui::TextUnformatted("Scene surface rendering is unavailable.");
                }
            }
            else
            {
                ImGui::TextUnformatted("Scene surface has no drawable area.");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
#else
        (void)isOpen;
        (void)services;
        (void)cameraTool;
#endif
    }

    const SceneViewportState& SceneViewportPanel::GetState() const noexcept
    {
        return m_State;
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

    Life::SceneRenderer2D::Scene2D SceneViewportPanel::BuildScene2D(const Life::Camera& camera) const
    {
        Life::SceneRenderer2D::Scene2D scene;
        scene.Camera = &camera;
        scene.Quads.reserve(3);

        Life::SceneRenderer2D::QuadCommand checkerQuad;
        checkerQuad.Position = { 0.0f, 0.0f, 0.0f };
        checkerQuad.Size = { 3.5f, 3.5f };
        checkerQuad.Color = m_CheckerTextureAsset
            ? glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
            : glm::vec4{ 1.0f, 0.0f, 1.0f, 1.0f };
        checkerQuad.TextureAsset = m_CheckerTextureAsset ? m_CheckerTextureAsset.get() : nullptr;
        scene.Quads.push_back(checkerQuad);

        Life::SceneRenderer2D::QuadCommand animatedQuad;
        animatedQuad.Position = { std::sin(m_ElapsedTime) * 1.75f, std::cos(m_ElapsedTime * 0.75f) * 1.25f, -0.5f };
        animatedQuad.Size = { 1.35f, 1.35f };
        animatedQuad.RotationRadians = m_ElapsedTime;
        animatedQuad.Color = { 0.95f, 0.45f, 0.25f, 0.90f };
        scene.Quads.push_back(animatedQuad);

        Life::SceneRenderer2D::QuadCommand accentQuad;
        accentQuad.Position = { -2.0f, -1.4f, -1.0f };
        accentQuad.Size = { 1.25f, 1.25f };
        accentQuad.Color = { 0.25f, 0.90f, 0.45f, 0.85f };
        scene.Quads.push_back(accentQuad);

        return scene;
    }

    bool SceneViewportPanel::RenderSceneSurface(uint32_t width, uint32_t height, const EditorServices& services, EditorCameraTool& cameraTool)
    {
        m_State.LastRenderSucceeded = false;

        if (!m_SceneSurface || !services.SceneRenderer2D || !services.CameraManager)
            return false;

        const float requestedAspectRatio = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
        services.CameraManager->get();
        cameraTool.Ensure(services.CameraManager->get(), requestedAspectRatio);

        auto editorCamera = cameraTool.TryGetCamera(services.CameraManager->get());
        if (!editorCamera)
            return false;

        Life::Camera& camera = editorCamera->get();

        if (!m_SceneSurface->Resize(width, height))
            return false;

        const float actualAspectRatio = m_SceneSurface->GetHeight() > 0
            ? static_cast<float>(m_SceneSurface->GetWidth()) / static_cast<float>(m_SceneSurface->GetHeight())
            : requestedAspectRatio;
        camera.SetAspectRatio(actualAspectRatio);

        if (!services.SceneRenderer2D->get().RenderToSurface(*m_SceneSurface, BuildScene2D(camera)))
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
