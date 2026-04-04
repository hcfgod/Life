#pragma once

#include "Engine.h"

#include <nlohmann/json.hpp>

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
        void OnEvent(Life::Event& event) override;

    private:
        nlohmann::json m_StartupConfig;
        float m_ElapsedTime = 0.0f;
        bool m_HasLoggedRuntime = false;
        bool m_WasMovementInputActive = false;
    };
}
