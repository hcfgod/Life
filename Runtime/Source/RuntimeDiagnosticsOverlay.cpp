#include "Runtime/RuntimeDiagnosticsOverlay.h"

namespace RuntimeApp
{
    RuntimeDiagnosticsOverlay::RuntimeDiagnosticsOverlay()
        : Life::Layer("RuntimeDiagnosticsOverlay")
    {
    }

    void RuntimeDiagnosticsOverlay::OnAttach()
    {
        LOG_INFO("Runtime diagnostics overlay attached.");
    }

    void RuntimeDiagnosticsOverlay::OnDetach()
    {
        LOG_INFO("Runtime diagnostics overlay detached.");
    }

    void RuntimeDiagnosticsOverlay::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowFocusGainedEvent>([](Life::WindowFocusGainedEvent&)
        {
            LOG_INFO("Runtime window focus gained.");
        });
        dispatcher.Dispatch<Life::WindowFocusLostEvent>([](Life::WindowFocusLostEvent&)
        {
            LOG_INFO("Runtime window focus lost.");
        });
        dispatcher.Dispatch<Life::WindowMinimizedEvent>([](Life::WindowMinimizedEvent&)
        {
            LOG_INFO("Runtime window minimized.");
        });
        dispatcher.Dispatch<Life::WindowRestoredEvent>([](Life::WindowRestoredEvent&)
        {
            LOG_INFO("Runtime window restored.");
        });
        dispatcher.Dispatch<Life::WindowCloseEvent>([](Life::WindowCloseEvent&)
        {
            LOG_INFO("Runtime window close requested.");
        });
    }
}
