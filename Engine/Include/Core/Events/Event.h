#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <stdexcept>
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

    enum class EventDispatchResult
    {
        Unhandled = 0,
        Handled,
        StopPropagation,
        HandledAndStopPropagation
    };

    enum class EventBusThreadingModel
    {
        OwnerThreadOnly
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

        bool IsHandled() const noexcept
        {
            return m_Handled;
        }

        bool MarkHandled() noexcept
        {
            const bool wasHandled = m_Handled;
            m_Handled = true;
            return !wasHandled;
        }

        bool IsPropagationStopped() const noexcept
        {
            return m_PropagationStopped;
        }

        bool StopPropagation() noexcept
        {
            const bool wasStopped = m_PropagationStopped;
            m_PropagationStopped = true;
            return !wasStopped;
        }

        void Accept() noexcept
        {
            MarkHandled();
            StopPropagation();
        }

    private:
        bool m_Handled = false;
        bool m_PropagationStopped = false;
    };

    using EventSubscriptionId = std::uint64_t;

    template<typename TEvent>
    struct EventSubscriptionOptions
    {
        int Priority = 0;
        std::function<bool(const TEvent&)> Filter;
    };

    namespace Detail
    {
        inline void ApplyEventDispatchResult(Event& event, EventDispatchResult result)
        {
            switch (result)
            {
            case EventDispatchResult::Unhandled:
                break;
            case EventDispatchResult::Handled:
                event.MarkHandled();
                break;
            case EventDispatchResult::StopPropagation:
                event.StopPropagation();
                break;
            case EventDispatchResult::HandledAndStopPropagation:
                event.Accept();
                break;
            default:
                break;
            }
        }

        template<typename TResult>
        EventDispatchResult NormalizeEventBusCallbackResult(TResult&& result)
        {
            using ResultType = std::remove_cvref_t<TResult>;
            if constexpr (std::is_same_v<ResultType, EventDispatchResult>)
            {
                return std::forward<TResult>(result);
            }
            else
            {
                return static_cast<bool>(std::forward<TResult>(result))
                    ? EventDispatchResult::HandledAndStopPropagation
                    : EventDispatchResult::Unhandled;
            }
        }

        template<typename TResult>
        EventDispatchResult NormalizeEventDispatcherCallbackResult(TResult&& result)
        {
            using ResultType = std::remove_cvref_t<TResult>;
            if constexpr (std::is_same_v<ResultType, EventDispatchResult>)
            {
                return std::forward<TResult>(result);
            }
            else
            {
                return static_cast<bool>(std::forward<TResult>(result))
                    ? EventDispatchResult::Handled
                    : EventDispatchResult::Unhandled;
            }
        }
    }

    class EventBus
    {
    public:
        EventBus() = default;

        explicit EventBus(EventBusThreadingModel threadingModel)
            : m_ThreadingModel(threadingModel)
        {
        }

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) = delete;
        EventBus& operator=(EventBus&&) = delete;

        EventBusThreadingModel GetThreadingModel() const noexcept
        {
            return m_ThreadingModel;
        }

        bool IsOwnerThread() const noexcept
        {
            return std::this_thread::get_id() == m_OwnerThreadId;
        }

        template<typename TEvent, typename TFunction>
        EventSubscriptionId Subscribe(TFunction&& function, EventSubscriptionOptions<TEvent> options = {})
        {
            static_assert(std::is_base_of_v<Event, TEvent>);
            EnsureThreadAffinity();

            const std::size_t eventTypeIndex = GetEventTypeIndex(TEvent::GetStaticType());
            if (eventTypeIndex >= m_Subscribers.size())
                throw std::out_of_range("EventBus received an invalid event type for subscription.");

            const EventSubscriptionId subscriptionId = m_NextSubscriptionId++;

            EventSubscription subscription;
            subscription.Id = subscriptionId;
            subscription.Priority = options.Priority;
            subscription.Callback = [callback = std::forward<TFunction>(function), filter = std::move(options.Filter)](Event& event) mutable
            {
                TEvent& typedEvent = static_cast<TEvent&>(event);
                if (filter && !filter(typedEvent))
                    return EventDispatchResult::Unhandled;

                if constexpr (std::is_void_v<std::invoke_result_t<decltype(callback)&, TEvent&>>)
                {
                    std::invoke(callback, typedEvent);
                    return EventDispatchResult::Unhandled;
                }
                else
                {
                    return Detail::NormalizeEventBusCallbackResult(std::invoke(callback, typedEvent));
                }
            };

            if (m_DispatchDepth > 0)
            {
                PendingSubscriptionMutation mutation;
                mutation.Type = PendingMutationType::Subscribe;
                mutation.EventTypeIndex = eventTypeIndex;
                mutation.Subscription = std::move(subscription);
                m_PendingMutations.emplace_back(std::move(mutation));
            }
            else
            {
                InsertSubscription(m_Subscribers[eventTypeIndex], std::move(subscription));
            }

            return subscriptionId;
        }

        bool Unsubscribe(EventSubscriptionId subscriptionId)
        {
            EnsureThreadAffinity();

            if (m_DispatchDepth > 0)
            {
                if (!HasSubscription(subscriptionId))
                    return false;

                PendingSubscriptionMutation mutation;
                mutation.Type = PendingMutationType::Unsubscribe;
                mutation.SubscriptionId = subscriptionId;
                m_PendingMutations.emplace_back(std::move(mutation));
                return true;
            }

            return RemoveSubscription(subscriptionId);
        }

        void Dispatch(Event& event)
        {
            EnsureThreadAffinity();

            if (event.IsPropagationStopped())
                return;

            const std::size_t eventTypeIndex = GetEventTypeIndex(event.GetEventType());
            if (eventTypeIndex >= m_Subscribers.size())
                throw std::out_of_range("EventBus received an invalid event type for dispatch.");

            DispatchScope dispatchScope(*this);
            auto& subscribers = m_Subscribers[eventTypeIndex];
            for (const EventSubscription& subscription : subscribers)
            {
                if (event.IsPropagationStopped())
                    break;

                Detail::ApplyEventDispatchResult(event, subscription.Callback(event));
            }
        }

        template<typename TEvent, typename... TArguments>
        bool Dispatch(TArguments&&... arguments)
        {
            static_assert(std::is_base_of_v<Event, TEvent>);

            TEvent event(std::forward<TArguments>(arguments)...);
            Dispatch(event);
            return event.IsHandled();
        }

    private:
        struct EventSubscription
        {
            EventSubscriptionId Id = 0;
            int Priority = 0;
            std::function<EventDispatchResult(Event&)> Callback;
        };

        enum class PendingMutationType
        {
            Subscribe,
            Unsubscribe
        };

        struct PendingSubscriptionMutation
        {
            PendingMutationType Type = PendingMutationType::Subscribe;
            std::size_t EventTypeIndex = 0;
            EventSubscription Subscription;
            EventSubscriptionId SubscriptionId = 0;
        };

        struct DispatchScope final
        {
            explicit DispatchScope(EventBus& eventBus)
                : EventBusInstance(eventBus)
            {
                ++EventBusInstance.m_DispatchDepth;
            }

            ~DispatchScope()
            {
                if (--EventBusInstance.m_DispatchDepth == 0)
                    EventBusInstance.ApplyPendingMutations();
            }

            EventBus& EventBusInstance;
        };

        static constexpr std::size_t GetEventTypeIndex(EventType eventType)
        {
            return static_cast<std::size_t>(eventType);
        }

        static void InsertSubscription(std::vector<EventSubscription>& subscribers, EventSubscription subscription)
        {
            const auto insertPosition = std::lower_bound(subscribers.begin(), subscribers.end(), subscription,
                [](const EventSubscription& existingSubscription, const EventSubscription& newSubscription)
                {
                    if (existingSubscription.Priority != newSubscription.Priority)
                        return existingSubscription.Priority > newSubscription.Priority;

                    return existingSubscription.Id < newSubscription.Id;
                });

            subscribers.insert(insertPosition, std::move(subscription));
        }

        static bool RemoveSubscription(std::vector<EventSubscription>& subscribers, EventSubscriptionId subscriptionId)
        {
            const auto iterator = std::remove_if(subscribers.begin(), subscribers.end(),
                [subscriptionId](const EventSubscription& subscription)
                {
                    return subscription.Id == subscriptionId;
                });

            if (iterator == subscribers.end())
                return false;

            subscribers.erase(iterator, subscribers.end());
            return true;
        }

        bool RemoveSubscription(EventSubscriptionId subscriptionId)
        {
            for (auto& subscribers : m_Subscribers)
            {
                if (RemoveSubscription(subscribers, subscriptionId))
                    return true;
            }

            return false;
        }

        bool HasSubscription(EventSubscriptionId subscriptionId) const
        {
            for (auto iterator = m_PendingMutations.rbegin(); iterator != m_PendingMutations.rend(); ++iterator)
            {
                if (iterator->Type == PendingMutationType::Unsubscribe && iterator->SubscriptionId == subscriptionId)
                    return false;

                if (iterator->Type == PendingMutationType::Subscribe && iterator->Subscription.Id == subscriptionId)
                    return true;
            }

            for (const auto& subscribers : m_Subscribers)
            {
                const auto iterator = std::find_if(subscribers.begin(), subscribers.end(),
                    [subscriptionId](const EventSubscription& subscription)
                    {
                        return subscription.Id == subscriptionId;
                    });

                if (iterator != subscribers.end())
                    return true;
            }

            return false;
        }

        void ApplyPendingMutations()
        {
            for (PendingSubscriptionMutation& mutation : m_PendingMutations)
            {
                if (mutation.Type == PendingMutationType::Subscribe)
                {
                    InsertSubscription(m_Subscribers[mutation.EventTypeIndex], std::move(mutation.Subscription));
                    continue;
                }

                RemoveSubscription(mutation.SubscriptionId);
            }

            m_PendingMutations.clear();
        }

        void EnsureThreadAffinity() const
        {
            if (m_ThreadingModel == EventBusThreadingModel::OwnerThreadOnly && !IsOwnerThread())
                throw std::logic_error("EventBus is owner-thread-only and must be accessed from the creating thread.");
        }

    private:
        std::array<std::vector<EventSubscription>, static_cast<std::size_t>(EventType::Count)> m_Subscribers;
        std::vector<PendingSubscriptionMutation> m_PendingMutations;
        EventSubscriptionId m_NextSubscriptionId = 1;
        EventBusThreadingModel m_ThreadingModel = EventBusThreadingModel::OwnerThreadOnly;
        std::thread::id m_OwnerThreadId = std::this_thread::get_id();
        std::size_t m_DispatchDepth = 0;
    };

    class EventDispatcher
    {
    public:
        explicit EventDispatcher(Event& event)
            : m_Event(event)
        {
        }

        template<typename TEvent, typename TFunction>
        bool Dispatch(TFunction&& function)
        {
            static_assert(std::is_base_of_v<Event, TEvent>);

            if (m_Event.GetEventType() != TEvent::GetStaticType())
                return false;

            if constexpr (std::is_void_v<std::invoke_result_t<TFunction, TEvent&>>)
            {
                std::invoke(std::forward<TFunction>(function), static_cast<TEvent&>(m_Event));
            }
            else
            {
                Detail::ApplyEventDispatchResult(
                    m_Event,
                    Detail::NormalizeEventDispatcherCallbackResult(
                        std::invoke(std::forward<TFunction>(function), static_cast<TEvent&>(m_Event))));
            }

            return true;
        }

    private:
        Event& m_Event;
    };
}
