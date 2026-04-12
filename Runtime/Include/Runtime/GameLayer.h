#pragma once

#include "Engine.h"

#include <nlohmann/json.hpp>
#include <string>

namespace RuntimeApp
{
    class GameLayer final : public Life::Layer
    {
    public:
        explicit GameLayer(const Life::ApplicationSpecification& specification);

    protected:
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(float timestep) override;
        void OnRender() override;
        void OnEvent(Life::Event& event) override;

    private:
        void TryAcquireCheckerTexture();
        void EnsureRuntimeScene();
        void PopulateRuntimeScene(Life::Scene& scene);
        void CacheServices();
        void ResetServices() noexcept;

        nlohmann::json m_StartupConfig;
        Life::OptionalRef<Life::InputSystem> m_InputSystem;
        Life::OptionalRef<Life::CameraManager> m_CameraManager;
        Life::OptionalRef<Life::SceneService> m_SceneService;
        Life::OptionalRef<Life::SceneRenderer2D> m_SceneRenderer2D;
        Life::Ref<Life::Assets::TextureAsset> m_CheckerTextureAsset;
        Life::Entity m_CheckerEntity;
        Life::Entity m_AnimatedEntity;
        Life::Entity m_AccentEntity;
        float m_ElapsedTime = 0.0f;
        bool m_HasLoggedRuntime = false;
        bool m_WasMovementInputActive = false;
        bool m_IsUsingPerspectiveCamera = false;
        std::string m_OrthographicCameraName = "Main2D";
        std::string m_PerspectiveCameraName = "PreviewPerspective";
        std::string m_CheckerTextureKey = "Assets/Textures/Renderer2DChecker.ppm";
    };
}
