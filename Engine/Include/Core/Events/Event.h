#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Life
{
    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResize,
        WindowMoved,
        WindowFocusGained,
        WindowFocusLost,
        WindowMinimized,
        WindowRestored,
        Count
    };

    enum class EventCategory
    {
        None = 0,
        Application = 1 << 0,
        Window = 1 << 1,
        Input = 1 << 2,
        Keyboard = 1 << 3,
        Mouse = 1 << 4,
        MouseButton = 1 << 5
    };

    class Event
    {
    public:
        virtual ~Event() = default;

        virtual EventType GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        bool IsInCategory(EventCategory category) const
        {
            return (GetCategoryFlags() & static_cast<int>(category)) != 0;
        }

    public:
        bool Handled = false;
    };

    using EventSubscriptionId = std::uint64_t;

    class EventBus
    {
    public:
        template<typename TEvent, typename TFunction>
        EventSubscriptionId Subscribe(TFunction&& function)
        {
            static_assert(std::is_base_of_v<Event, TEvent>);

            const std::size_t eventTypeIndex = GetEventTypeIndex(TEvent::GetStaticType());
            EventSubscription subscription;
            subscription.Id = m_NextSubscriptionId++;
            subscription.Callback = [callback = std::forward<TFunction>(function)](Event& event) mutable
            {
                if constexpr (std::is_void_v<std::invoke_result_t<decltype(callback)&, TEvent&>>)
                {
                    callback(static_cast<TEvent&>(event));
                    return false;
                }

                return static_cast<bool>(callback(static_cast<TEvent&>(event)));
            };

            m_Subscribers[eventTypeIndex].emplace_back(std::move(subscription));
            return m_Subscribers[eventTypeIndex].back().Id;
        }

        bool Unsubscribe(EventSubscriptionId subscriptionId)
        {
            for (auto& subscribers : m_Subscribers)
            {
                auto iterator = std::remove_if(subscribers.begin(), subscribers.end(),
                    [subscriptionId](const EventSubscription& subscription)
                    {
                        return subscription.Id == subscriptionId;
                    });

                if (iterator == subscribers.end())
                    continue;

                subscribers.erase(iterator, subscribers.end());
                return true;
            }

            return false;
        }

        void Dispatch(Event& event)
        {
            if (event.Handled)
                return;

            const std::size_t eventTypeIndex = GetEventTypeIndex(event.GetEventType());
            const auto subscribers = m_Subscribers[eventTypeIndex];
            for (const EventSubscription& subscription : subscribers)
            {
                if (event.Handled)
                    break;

                event.Handled |= subscription.Callback(event);
            }
        }

        template<typename TEvent, typename... TArguments>
        bool Dispatch(TArguments&&... arguments)
        {
            static_assert(std::is_base_of_v<Event, TEvent>);

            TEvent event(std::forward<TArguments>(arguments)...);
            Dispatch(event);
            return event.Handled;
        }

    private:
        struct EventSubscription
        {
            EventSubscriptionId Id = 0;
            std::function<bool(Event&)> Callback;
        };

        static constexpr std::size_t GetEventTypeIndex(EventType eventType)
        {
            return static_cast<std::size_t>(eventType);
        }

    private:
        std::array<std::vector<EventSubscription>, static_cast<std::size_t>(EventType::Count)> m_Subscribers;
        EventSubscriptionId m_NextSubscriptionId = 1;
    };

    class EventDispatcher
    {
    public:
        explicit EventDispatcher(Event& event)
            : m_Event(event)
        {
        }

        template<typename TEvent, typename TFunction>
        bool Dispatch(const TFunction& function)
        {
            static_assert(std::is_base_of_v<Event, TEvent>);

            if (m_Event.GetEventType() != TEvent::GetStaticType())
                return false;

            m_Event.Handled |= function(static_cast<TEvent&>(m_Event));
            return true;
        }

    private:
        Event& m_Event;
    };
}
