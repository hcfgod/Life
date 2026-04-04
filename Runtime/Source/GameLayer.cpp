#include "Runtime/GameLayer.h"
#include "Graphics/Renderer.h"
#include "Graphics/RenderCommand.h"

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
        LOG_INFO("Game layer attached.");
    }

    void GameLayer::OnDetach()
    {
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

        if (auto* renderer = GetApplication().TryGetService<Life::Renderer>())
        {
            Life::RenderCommand::Clear(*renderer, 0.392f, 0.584f, 0.929f, 1.0f);
        }
    }

    void GameLayer::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& resizeEvent)
        {
            LOG_INFO("Runtime window resized to {}x{}.", resizeEvent.GetWidth(), resizeEvent.GetHeight());
        });
        dispatcher.Dispatch<Life::WindowMovedEvent>([&](Life::WindowMovedEvent& movedEvent)
        {
            LOG_INFO("Runtime window moved to {}, {}.", movedEvent.GetX(), movedEvent.GetY());
        });
    }
}
