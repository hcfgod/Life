#include "Core/ApplicationEventRouter.h"

#include "Core/Application.h"

namespace Life
{
    void ApplicationEventRouter::Route(Application& application, Event& event)
    {
        application.OnEvent(event);
        m_EventBus.Dispatch(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([&application](WindowCloseEvent& closeEvent)
        {
            if (closeEvent.Handled)
                return false;

            application.Shutdown();
            return true;
        });
    }
}
