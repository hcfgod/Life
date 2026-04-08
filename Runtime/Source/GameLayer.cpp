#include "Runtime/GameLayer.h"

#include <cmath>

namespace RuntimeApp
{
    GameLayer::GameLayer(const Life::ApplicationSpecification& specification)
        : Life::Layer("GameLayer")
        , m_StartupConfig(
            {
                { "name", specification.Name },
                { "width", specification.Width },
                { "height", specification.Height },
                { "vsync", specification.VSync }
            })
    {
    }

    void GameLayer::TryAcquireCheckerTexture()
    {
        if (m_CheckerTextureAsset)
            return;

        if (auto* assetManager = GetApplication().TryGetService<Life::Assets::AssetManager>())
        {
            m_CheckerTextureAsset = assetManager->GetOrLoad<Life::Assets::TextureAsset>(m_CheckerTextureKey);
            if (!m_CheckerTextureAsset)
                return;

            m_CheckerTextureAsset->SetFilterModes(Life::TextureFilterMode::Nearest, Life::TextureFilterMode::Nearest);
            m_CheckerTextureAsset->SetWrapModes(Life::TextureWrapMode::Repeat, Life::TextureWrapMode::Repeat);
            LOG_INFO("Runtime recovered textured quad asset '{}'.", m_CheckerTextureKey);
        }
    }

    void GameLayer::OnAttach()
    {
        CacheServices();

        TryAcquireCheckerTexture();
        if (!m_CheckerTextureAsset)
            LOG_WARN("Runtime failed to load textured quad asset '{}'. Falling back to error texture.", m_CheckerTextureKey);

        LOG_INFO("Runtime boot config: {}", m_StartupConfig.dump());
        if (m_CameraManager)
        {
            const auto& specification = GetApplication().GetSpecification();
            Life::CameraManager& cameraManager = m_CameraManager.value().get();
            const float aspectRatio = specification.Height > 0
                ? static_cast<float>(specification.Width) / static_cast<float>(specification.Height)
                : 1.0f;

            Life::CameraSpecification orthographicCameraSpec;
            orthographicCameraSpec.Name = m_OrthographicCameraName;
            orthographicCameraSpec.Projection = Life::ProjectionType::Orthographic;
            orthographicCameraSpec.AspectRatio = aspectRatio;
            orthographicCameraSpec.OrthoSize = 4.5f;
            orthographicCameraSpec.OrthoNear = 0.1f;
            orthographicCameraSpec.OrthoFar = 10.0f;
            orthographicCameraSpec.Priority = 0;
            orthographicCameraSpec.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };

            if (Life::Camera* orthographicCamera = cameraManager.CreateCamera(orthographicCameraSpec))
            {
                orthographicCamera->SetPosition({ 0.0f, 0.0f, 1.0f });
                orthographicCamera->LookAt({ 0.0f, 0.0f, 0.0f });
            }

            Life::CameraSpecification perspectiveCameraSpec;
            perspectiveCameraSpec.Name = m_PerspectiveCameraName;
            perspectiveCameraSpec.Projection = Life::ProjectionType::Perspective;
            perspectiveCameraSpec.AspectRatio = aspectRatio;
            perspectiveCameraSpec.FieldOfView = 50.0f;
            perspectiveCameraSpec.NearClip = 0.1f;
            perspectiveCameraSpec.FarClip = 100.0f;
            perspectiveCameraSpec.Priority = 1;
            perspectiveCameraSpec.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };

            if (Life::Camera* perspectiveCamera = cameraManager.CreateCamera(perspectiveCameraSpec))
            {
                perspectiveCamera->SetPosition({ 0.0f, 0.0f, 6.0f });
                perspectiveCamera->LookAt({ 0.0f, 0.0f, 0.0f });
            }

            cameraManager.SetPrimaryCamera(m_OrthographicCameraName);
        }

        LOG_INFO("Game layer attached.");
    }

    void GameLayer::OnDetach()
    {
        if (m_CameraManager)
        {
            Life::CameraManager& cameraManager = m_CameraManager.value().get();
            cameraManager.DestroyCamera(m_OrthographicCameraName);
            cameraManager.DestroyCamera(m_PerspectiveCameraName);
        }

        m_CheckerTextureAsset.reset();

        ResetServices();

        LOG_INFO("Game layer detached.");
    }

    void GameLayer::OnUpdate(float timestep)
    {
        m_ElapsedTime += timestep;

        TryAcquireCheckerTexture();

        if (!m_HasLoggedRuntime && m_ElapsedTime >= 1.0f)
        {
            LOG_INFO("Runtime running.");
            m_HasLoggedRuntime = true;
        }

        if (m_InputSystem)
        {
            Life::InputSystem& input = m_InputSystem.value().get();

            if (input.WasActionStartedThisFrame("Gameplay", "Quit"))
            {
                LOG_INFO("Quit action triggered.");
                GetApplication().RequestShutdown();
            }

            const Life::InputVector2 movement = input.ReadActionAxis2D("Gameplay", "Move");
            const bool movementActive = std::abs(movement.x) > 0.01f || std::abs(movement.y) > 0.01f;
            if (movementActive && !m_WasMovementInputActive)
            {
                LOG_INFO("Movement input detected: ({}, {}).", movement.x, movement.y);
            }
            else if (!movementActive && m_WasMovementInputActive)
            {
                LOG_INFO("Movement input released.");
            }

            m_WasMovementInputActive = movementActive;

            if (input.WasActionStartedThisFrame("Gameplay", "ToggleCamera") && m_CameraManager)
            {
                Life::CameraManager& cameraManager = m_CameraManager.value().get();
                m_IsUsingPerspectiveCamera = !m_IsUsingPerspectiveCamera;
                const std::string& activeCameraName = m_IsUsingPerspectiveCamera
                    ? m_PerspectiveCameraName
                    : m_OrthographicCameraName;

                if (cameraManager.SetPrimaryCamera(activeCameraName))
                    LOG_INFO("Active camera switched to '{}'.", activeCameraName);
            }
        }
    }

    void GameLayer::OnRender()
    {
        if (m_CameraManager && m_Renderer2D)
        {
            Life::CameraManager& cameraManager = m_CameraManager.value().get();
            Life::Renderer2D& renderer2D = m_Renderer2D.value().get();
            if (Life::Camera* activeCamera = cameraManager.GetPrimaryCamera())
            {
                renderer2D.BeginScene(*activeCamera);
                if (m_CheckerTextureAsset)
                    renderer2D.DrawQuad({ 0.0f, 0.0f, 0.0f }, { 3.5f, 3.5f }, *m_CheckerTextureAsset, { 1.0f, 1.0f, 1.0f, 1.0f });
                else
                    renderer2D.DrawQuad({ 0.0f, 0.0f, 0.0f }, { 3.5f, 3.5f }, { 1.0f, 0.0f, 1.0f, 1.0f });
                renderer2D.DrawRotatedQuad(
                    { std::sin(m_ElapsedTime) * 1.75f, std::cos(m_ElapsedTime * 0.75f) * 1.25f, -0.5f },
                    { 1.35f, 1.35f },
                    m_ElapsedTime,
                    { 0.95f, 0.45f, 0.25f, 0.90f });
                renderer2D.DrawQuad({ -2.0f, -1.4f, -1.0f }, { 1.25f, 1.25f }, { 0.25f, 0.90f, 0.45f, 0.85f });
                renderer2D.EndScene();
            }
        }
    }

    void GameLayer::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& resizeEvent)
        {
            LOG_INFO("Runtime window resized to {}x{}.", resizeEvent.GetWidth(), resizeEvent.GetHeight());
            if (m_CameraManager)
            {
                Life::CameraManager& cameraManager = m_CameraManager.value().get();
                const float aspectRatio = resizeEvent.GetHeight() > 0
                    ? static_cast<float>(resizeEvent.GetWidth()) / static_cast<float>(resizeEvent.GetHeight())
                    : 1.0f;
                cameraManager.SetAspectRatioAll(aspectRatio);
            }
            return false;
        });
        dispatcher.Dispatch<Life::WindowMovedEvent>([&](Life::WindowMovedEvent& movedEvent)
        {
            LOG_INFO("Runtime window moved to {}, {}.", movedEvent.GetX(), movedEvent.GetY());
            return false;
        });
    }

    void GameLayer::CacheServices()
    {
        m_InputSystem = Life::MakeOptionalRef(GetApplication().GetService<Life::InputSystem>());
        m_CameraManager = Life::MakeOptionalRef(GetApplication().GetService<Life::CameraManager>());

        if (GetApplication().HasService<Life::Renderer2D>())
            m_Renderer2D = Life::MakeOptionalRef(GetApplication().GetService<Life::Renderer2D>());
        else
            m_Renderer2D.reset();
    }

    void GameLayer::ResetServices() noexcept
    {
        m_Renderer2D.reset();
        m_CameraManager.reset();
        m_InputSystem.reset();
    }
}
