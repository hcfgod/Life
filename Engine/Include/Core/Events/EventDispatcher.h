#pragma once

#include "Core/Events/EventDispatch.h"

#include <functional>
#include <type_traits>
#include <utility>

namespace Life
{
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
