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
        nlohmann::json m_StartupConfig;
        float m_ElapsedTime = 0.0f;
        bool m_HasLoggedRuntime = false;
        bool m_WasMovementInputActive = false;
        bool m_IsUsingPerspectiveCamera = false;
        std::string m_OrthographicCameraName = "Main2D";
        std::string m_PerspectiveCameraName = "PreviewPerspective";
    };
}
