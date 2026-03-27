#include "Core/ApplicationEventRouter.h"

#include "Core/Application.h"
#include "Core/LayerStack.h"

namespace Life
{
    void ApplicationEventRouter::Route(Application& application, Event& event)
    {
        application.OnEvent(event);
        if (event.IsPropagationStopped())
            return;

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
        dispatcher.Dispatch<WindowCloseEvent>([&application](WindowCloseEvent& closeEvent)
        {
            if (closeEvent.IsHandled())
                return EventDispatchResult::Unhandled;

            application.RequestShutdown();
            return EventDispatchResult::HandledAndStopPropagation;
        });
    }
}
