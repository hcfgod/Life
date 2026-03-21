#pragma once

#include "Core/Events/ApplicationEvent.h"
#include "Core/Memory.h"

#include <SDL3/SDL.h>

namespace Life
{
    inline Scope<Event> TranslateSDLEvent(const SDL_Event& sdlEvent)
    {
        switch (sdlEvent.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                return CreateScope<WindowCloseEvent>();

            case SDL_EVENT_WINDOW_RESIZED:
                return CreateScope<WindowResizeEvent>(
                    static_cast<uint32_t>(sdlEvent.window.data1),
                    static_cast<uint32_t>(sdlEvent.window.data2)
                );

            case SDL_EVENT_WINDOW_MOVED:
                return CreateScope<WindowMovedEvent>(sdlEvent.window.data1, sdlEvent.window.data2);

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                return CreateScope<WindowFocusGainedEvent>();

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                return CreateScope<WindowFocusLostEvent>();

            case SDL_EVENT_WINDOW_MINIMIZED:
                return CreateScope<WindowMinimizedEvent>();

            case SDL_EVENT_WINDOW_RESTORED:
                return CreateScope<WindowRestoredEvent>();

            default:
                return nullptr;
        }
    }
}
