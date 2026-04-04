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

    void GameLayer::OnAttach()
    {
        LOG_INFO("Runtime boot config: {}", m_StartupConfig.dump());
        if (auto* cameraManager = GetApplication().TryGetService<Life::CameraManager>())
        {
            const auto& specification = GetApplication().GetSpecification();
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

            if (Life::Camera* orthographicCamera = cameraManager->CreateCamera(orthographicCameraSpec))
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

            if (Life::Camera* perspectiveCamera = cameraManager->CreateCamera(perspectiveCameraSpec))
            {
                perspectiveCamera->SetPosition({ 0.0f, 0.0f, 6.0f });
                perspectiveCamera->LookAt({ 0.0f, 0.0f, 0.0f });
            }

            cameraManager->SetPrimaryCamera(m_OrthographicCameraName);
        }

        LOG_INFO("Game layer attached.");
    }

    void GameLayer::OnDetach()
    {
        if (auto* cameraManager = GetApplication().TryGetService<Life::CameraManager>())
        {
            cameraManager->DestroyCamera(m_OrthographicCameraName);
            cameraManager->DestroyCamera(m_PerspectiveCameraName);
        }

        LOG_INFO("Game layer detached.");
    }

    void GameLayer::OnUpdate(float timestep)
    {
        m_ElapsedTime += timestep;

        if (!m_HasLoggedRuntime && m_ElapsedTime >= 1.0f)
        {
            LOG_INFO("Runtime running.");
            m_HasLoggedRuntime = true;
        }

        if (auto* input = GetApplication().TryGetService<Life::InputSystem>())
        {
            if (input->WasActionStartedThisFrame("Gameplay", "Quit"))
            {
                LOG_INFO("Quit action triggered.");
                GetApplication().RequestShutdown();
            }

            const Life::InputVector2 movement = input->ReadActionAxis2D("Gameplay", "Move");
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
        }

        if (auto* cameraManager = GetApplication().TryGetService<Life::CameraManager>())
        {
            const bool shouldUsePerspectiveCamera = std::fmod(m_ElapsedTime, 6.0f) >= 3.0f;
            if (shouldUsePerspectiveCamera != m_IsUsingPerspectiveCamera)
            {
                m_IsUsingPerspectiveCamera = shouldUsePerspectiveCamera;
                const std::string& activeCameraName = m_IsUsingPerspectiveCamera
                    ? m_PerspectiveCameraName
                    : m_OrthographicCameraName;

                if (cameraManager->SetPrimaryCamera(activeCameraName))
                    LOG_INFO("Active camera switched to '{}'.", activeCameraName);
            }
        }
    }

    void GameLayer::OnRender()
    {
        if (auto* cameraManager = GetApplication().TryGetService<Life::CameraManager>())
        {
            if (auto* renderer2D = GetApplication().TryGetService<Life::Renderer2D>())
            {
                if (Life::Camera* activeCamera = cameraManager->GetPrimaryCamera())
                {
                    renderer2D->BeginScene(*activeCamera);
                    renderer2D->DrawQuad({ 0.0f, 0.0f, 0.0f }, { 3.5f, 3.5f }, { 0.20f, 0.55f, 0.95f, 1.0f });
                    renderer2D->DrawRotatedQuad(
                        { std::sin(m_ElapsedTime) * 1.75f, std::cos(m_ElapsedTime * 0.75f) * 1.25f, -0.5f },
                        { 1.35f, 1.35f },
                        m_ElapsedTime,
                        { 0.95f, 0.45f, 0.25f, 0.90f });
                    renderer2D->DrawQuad({ -2.0f, -1.4f, -1.0f }, { 1.25f, 1.25f }, { 0.25f, 0.90f, 0.45f, 0.85f });
                    renderer2D->EndScene();
                }
            }
        }
    }

    void GameLayer::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& resizeEvent)
        {
            LOG_INFO("Runtime window resized to {}x{}.", resizeEvent.GetWidth(), resizeEvent.GetHeight());
            if (auto* cameraManager = GetApplication().TryGetService<Life::CameraManager>())
            {
                const float aspectRatio = resizeEvent.GetHeight() > 0
                    ? static_cast<float>(resizeEvent.GetWidth()) / static_cast<float>(resizeEvent.GetHeight())
                    : 1.0f;
                cameraManager->SetAspectRatioAll(aspectRatio);
            }
            return false;
        });
        dispatcher.Dispatch<Life::WindowMovedEvent>([&](Life::WindowMovedEvent& movedEvent)
        {
            LOG_INFO("Runtime window moved to {}, {}.", movedEvent.GetX(), movedEvent.GetY());
            return false;
        });
    }
}
