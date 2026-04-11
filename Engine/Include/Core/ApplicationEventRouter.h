#pragma once

#include "Core/Events/EventBus.h"

#include <utility>

namespace Life
{
    class Application;

    class ApplicationEventRouter
    {
    public:
        template<typename TEvent, typename TFunction>
        EventSubscriptionId Subscribe(TFunction&& function, EventSubscriptionOptions<TEvent> options = {})
        {
            return m_EventBus.Subscribe<TEvent>(std::forward<TFunction>(function), std::move(options));
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
