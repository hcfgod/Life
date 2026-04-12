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

            if (m_CheckerEntity.IsValid())
            {
                if (Life::SpriteComponent* sprite = m_CheckerEntity.TryGetComponent<Life::SpriteComponent>())
                {
                    sprite->TextureAssetKey = m_CheckerTextureKey;
                    sprite->TextureAsset = m_CheckerTextureAsset;
                }
            }
        }
    }

    void GameLayer::EnsureRuntimeScene()
    {
        if (!m_SceneService)
            return;

        Life::SceneService& sceneService = m_SceneService->get();
        if (!sceneService.HasActiveScene())
        {
            Life::Scene& scene = sceneService.CreateScene("RuntimeScene");
            PopulateRuntimeScene(scene);
            return;
        }

        Life::Scene& scene = sceneService.GetActiveScene();
        if (!m_CheckerEntity.IsValid() || !m_AnimatedEntity.IsValid() || !m_AccentEntity.IsValid())
            PopulateRuntimeScene(scene);
    }

    void GameLayer::PopulateRuntimeScene(Life::Scene& scene)
    {
        scene.Clear();
        scene.SetState(Life::Scene::State::Ready);

        m_CheckerEntity = scene.CreateEntity("CheckerQuad");
        m_CheckerEntity.GetComponent<Life::TransformComponent>().LocalPosition = { 0.0f, 0.0f, 0.0f };
        Life::SpriteComponent checkerSprite;
        checkerSprite.Size = { 3.5f, 3.5f };
        checkerSprite.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        checkerSprite.TextureAssetKey = m_CheckerTextureKey;
        checkerSprite.TextureAsset = m_CheckerTextureAsset;
        m_CheckerEntity.AddComponent<Life::SpriteComponent>(checkerSprite);

        m_AnimatedEntity = scene.CreateEntity("AnimatedQuad");
        m_AnimatedEntity.GetComponent<Life::TransformComponent>().LocalPosition = { 0.0f, 0.0f, -0.5f };
        Life::SpriteComponent animatedSprite;
        animatedSprite.Size = { 1.35f, 1.35f };
        animatedSprite.Color = { 0.95f, 0.45f, 0.25f, 0.90f };
        m_AnimatedEntity.AddComponent<Life::SpriteComponent>(animatedSprite);

        m_AccentEntity = scene.CreateEntity("AccentQuad");
        m_AccentEntity.GetComponent<Life::TransformComponent>().LocalPosition = { -2.0f, -1.4f, -1.0f };
        Life::SpriteComponent accentSprite;
        accentSprite.Size = { 1.25f, 1.25f };
        accentSprite.Color = { 0.25f, 0.90f, 0.45f, 0.85f };
        m_AccentEntity.AddComponent<Life::SpriteComponent>(accentSprite);
    }

    void GameLayer::OnAttach()
    {
        CacheServices();
        EnsureRuntimeScene();

        TryAcquireCheckerTexture();
        if (!m_CheckerTextureAsset)
            LOG_WARN("Runtime failed to load textured quad asset '{}'. Falling back to error texture.", m_CheckerTextureKey);

        LOG_INFO("Runtime boot config: {}", m_StartupConfig.dump());
        const Life::OptionalRef<Life::CameraManager> cameraManagerRef = m_CameraManager;
        if (cameraManagerRef.has_value())
        {
            Life::CameraManager& cameraManager = cameraManagerRef.value().get();
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
        const Life::OptionalRef<Life::CameraManager> cameraManagerRef = m_CameraManager;
        if (cameraManagerRef.has_value())
        {
            Life::CameraManager& cameraManager = cameraManagerRef.value().get();
            cameraManager.DestroyCamera(m_OrthographicCameraName);
            cameraManager.DestroyCamera(m_PerspectiveCameraName);
        }

        m_CheckerTextureAsset.reset();
        m_CheckerEntity = {};
        m_AnimatedEntity = {};
        m_AccentEntity = {};

        ResetServices();

        LOG_INFO("Game layer detached.");
    }

    void GameLayer::OnUpdate(float timestep)
    {
        m_ElapsedTime += timestep;

        TryAcquireCheckerTexture();
        EnsureRuntimeScene();

        if (m_AnimatedEntity.IsValid())
        {
            auto& transform = m_AnimatedEntity.GetComponent<Life::TransformComponent>();
            transform.LocalPosition = { std::sin(m_ElapsedTime) * 1.75f, std::cos(m_ElapsedTime * 0.75f) * 1.25f, -0.5f };
            transform.LocalRotation.z = m_ElapsedTime;
        }

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

            if (input.WasActionStartedThisFrame("Gameplay", "ToggleCamera"))
            {
                const Life::OptionalRef<Life::CameraManager> cameraManagerRef = m_CameraManager;
                if (cameraManagerRef.has_value())
                {
                    Life::CameraManager& cameraManager = cameraManagerRef.value().get();
                    m_IsUsingPerspectiveCamera = !m_IsUsingPerspectiveCamera;
                    const std::string& activeCameraName = m_IsUsingPerspectiveCamera
                        ? m_PerspectiveCameraName
                        : m_OrthographicCameraName;

                    if (cameraManager.SetPrimaryCamera(activeCameraName))
                        LOG_INFO("Active camera switched to '{}'.", activeCameraName);
                }
            }
        }
    }

    void GameLayer::OnRender()
    {
        if (m_SceneRenderer2D)
        {
            const Life::OptionalRef<Life::CameraManager> cameraManagerRef = m_CameraManager;
            if (cameraManagerRef.has_value())
            {
                Life::CameraManager& cameraManager = cameraManagerRef.value().get();
                if (Life::Camera* activeCamera = cameraManager.GetPrimaryCamera())
                {
                    if (m_SceneService && m_SceneService->get().HasActiveScene())
                        m_SceneRenderer2D->get().Render(m_SceneService->get().GetActiveScene(), *activeCamera);
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
            const Life::OptionalRef<Life::CameraManager> cameraManagerRef = m_CameraManager;
            if (cameraManagerRef.has_value())
            {
                Life::CameraManager& cameraManager = cameraManagerRef.value().get();
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
        m_SceneService = Life::MakeOptionalRef(GetApplication().GetService<Life::SceneService>());

        if (GetApplication().HasService<Life::SceneRenderer2D>())
            m_SceneRenderer2D = Life::MakeOptionalRef(GetApplication().GetService<Life::SceneRenderer2D>());
        else
            m_SceneRenderer2D.reset();
    }

    void GameLayer::ResetServices() noexcept
    {
        m_SceneRenderer2D.reset();
        m_SceneService.reset();
        m_CameraManager.reset();
        m_InputSystem.reset();
    }
}
