#include "Core/ApplicationEventRouter.h"

#include "Core/Application.h"
#include "Core/Events/ApplicationEvent.h"
#include "Core/Events/EventDispatcher.h"
#include "Core/LayerStack.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ImGuiSystem.h"

namespace Life
{
    void ApplicationEventRouter::Route(Application& application, Event& event)
    {
        application.OnEvent(event);
        if (event.IsPropagationStopped())
            return;

        if (ImGuiSystem* imguiSystem = application.TryGetService<ImGuiSystem>())
        {
            imguiSystem->CaptureEvent(event);
            if (event.IsPropagationStopped())
                return;
        }

        if (LayerStack* layerStack = application.TryGetService<LayerStack>())
        {
            layerStack->OnEvent(event);
            if (event.IsPropagationStopped())
                return;
        }

        m_EventBus.Dispatch(event);
        if (event.IsPropagationStopped())
            return;

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>([&application](WindowResizeEvent& resizeEvent)
        {
            if (GraphicsDevice* graphicsDevice = application.TryGetService<GraphicsDevice>())
                graphicsDevice->Resize(resizeEvent.GetWidth(), resizeEvent.GetHeight());

            return EventDispatchResult::Unhandled;
        });

        if (event.IsPropagationStopped())
            return;

        dispatcher.Dispatch<WindowCloseEvent>([&application](WindowCloseEvent& closeEvent)
        {
            if (closeEvent.IsHandled())
                return EventDispatchResult::Unhandled;

            application.RequestShutdown();
            return EventDispatchResult::HandledAndStopPropagation;
        });
    }
}
