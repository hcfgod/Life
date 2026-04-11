#pragma once

#include "Core/Events/EventBase.h"

#include <type_traits>
#include <utility>

namespace Life
{
    enum class EventDispatchResult
    {
        Unhandled = 0,
        Handled,
        StopPropagation,
        HandledAndStopPropagation
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
}
