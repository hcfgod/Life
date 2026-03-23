#pragma once

#include "Core/Events/ApplicationEvent.h"
#include "Core/Events/Event.h"

#include <utility>

namespace Life
{
    class Application;

    class ApplicationEventRouter
    {
    public:
        template<typename TEvent, typename TFunction>
        EventSubscriptionId Subscribe(TFunction&& function)
        {
            return m_EventBus.Subscribe<TEvent>(std::forward<TFunction>(function));
        }

        bool Unsubscribe(EventSubscriptionId subscriptionId)
        {
            return m_EventBus.Unsubscribe(subscriptionId);
        }

        void Route(Application& application, Event& event);

    private:
        EventBus m_EventBus;
    };
}
