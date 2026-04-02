#include "Runtime/GameLayer.h"
#include "Graphics/GraphicsDevice.h"

#include <nvrhi/nvrhi.h>

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

        if (auto* graphics = GetApplication().TryGetService<Life::GraphicsDevice>())
        {
            nvrhi::ITexture* backBuffer = graphics->GetCurrentBackBuffer();
            nvrhi::ICommandList* commandList = graphics->GetCurrentCommandList();

            if (backBuffer && commandList)
            {
                nvrhi::Color clearColor(0.392f, 0.584f, 0.929f, 1.0f);
                commandList->clearTextureFloat(backBuffer, nvrhi::AllSubresources, clearColor);
            }
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
