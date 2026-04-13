#pragma once

#include "Core/Events/ApplicationEvent.h"
#include "Core/Events/InputEvent.h"
#include "Core/Memory.h"
#include "Platform/SDL/SDLInputCodes.h"

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

            case SDL_EVENT_DROP_FILE:
                return CreateScope<WindowFileDroppedEvent>(
                    std::filesystem::path(sdlEvent.drop.data != nullptr ? sdlEvent.drop.data : ""),
                    sdlEvent.drop.x,
                    sdlEvent.drop.y
                );

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                return CreateScope<WindowFocusGainedEvent>();

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                return CreateScope<WindowFocusLostEvent>();

            case SDL_EVENT_WINDOW_MINIMIZED:
                return CreateScope<WindowMinimizedEvent>();

            case SDL_EVENT_WINDOW_RESTORED:
                return CreateScope<WindowRestoredEvent>();

            case SDL_EVENT_KEY_DOWN:
                return CreateScope<KeyPressedEvent>(ToKeyCode(sdlEvent.key.scancode), sdlEvent.key.repeat);

            case SDL_EVENT_KEY_UP:
                return CreateScope<KeyReleasedEvent>(ToKeyCode(sdlEvent.key.scancode));

            case SDL_EVENT_MOUSE_MOTION:
                return CreateScope<MouseMovedEvent>(sdlEvent.motion.x, sdlEvent.motion.y, sdlEvent.motion.xrel, sdlEvent.motion.yrel);

            case SDL_EVENT_MOUSE_WHEEL:
                return CreateScope<MouseScrolledEvent>(sdlEvent.wheel.x, sdlEvent.wheel.y);

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                return CreateScope<MouseButtonPressedEvent>(ToMouseButtonCode(sdlEvent.button.button));

            case SDL_EVENT_MOUSE_BUTTON_UP:
                return CreateScope<MouseButtonReleasedEvent>(ToMouseButtonCode(sdlEvent.button.button));

            case SDL_EVENT_GAMEPAD_ADDED:
                return CreateScope<GamepadAddedEvent>(sdlEvent.gdevice.which);

            case SDL_EVENT_GAMEPAD_REMOVED:
                return CreateScope<GamepadRemovedEvent>(sdlEvent.gdevice.which);

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                return CreateScope<GamepadButtonPressedEvent>(sdlEvent.gbutton.which, ToGamepadButtonCode(static_cast<SDL_GamepadButton>(sdlEvent.gbutton.button)));

            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                return CreateScope<GamepadButtonReleasedEvent>(sdlEvent.gbutton.which, ToGamepadButtonCode(static_cast<SDL_GamepadButton>(sdlEvent.gbutton.button)));

            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                return CreateScope<GamepadAxisMovedEvent>(sdlEvent.gaxis.which, ToGamepadAxisCode(static_cast<SDL_GamepadAxis>(sdlEvent.gaxis.axis)), sdlEvent.gaxis.value);

            default:
                return nullptr;
        }
    }
}
