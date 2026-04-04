#pragma once

#include "Core/Input/InputCodes.h"

#include <SDL3/SDL.h>

namespace Life
{
    inline KeyCode ToKeyCode(SDL_Scancode scancode)
    {
        const int rawValue = static_cast<int>(scancode);
        if (rawValue <= 0 || rawValue >= static_cast<int>(InputCodeLimits::KeyCount))
            return KeyCodes::Unknown;
        return KeyCode{ static_cast<uint16_t>(rawValue) };
    }

    inline SDL_Scancode ToSDLScancode(KeyCode keyCode)
    {
        const uint16_t rawValue = keyCode.GetValue();
        if (rawValue == 0 || rawValue >= static_cast<uint16_t>(SDL_SCANCODE_COUNT))
            return SDL_SCANCODE_UNKNOWN;
        return static_cast<SDL_Scancode>(rawValue);
    }

    inline MouseButtonCode ToMouseButtonCode(uint8_t button)
    {
        if (button >= static_cast<uint8_t>(InputCodeLimits::MouseButtonCount))
            return MouseButtons::None;
        return MouseButtonCode{ button };
    }

    inline uint8_t ToSDLMouseButton(MouseButtonCode button)
    {
        return button.GetValue();
    }

    inline GamepadButtonCode ToGamepadButtonCode(SDL_GamepadButton button)
    {
        const int rawValue = static_cast<int>(button);
        if (rawValue < 0 || rawValue >= static_cast<int>(InputCodeLimits::GamepadButtonCount))
            return GamepadButtons::Invalid;
        return GamepadButtonCode{ static_cast<int16_t>(rawValue) };
    }

    inline SDL_GamepadButton ToSDLGamepadButton(GamepadButtonCode button)
    {
        const int16_t rawValue = button.GetValue();
        if (rawValue < 0 || rawValue >= static_cast<int16_t>(SDL_GAMEPAD_BUTTON_COUNT))
            return SDL_GAMEPAD_BUTTON_INVALID;
        return static_cast<SDL_GamepadButton>(rawValue);
    }

    inline GamepadAxisCode ToGamepadAxisCode(SDL_GamepadAxis axis)
    {
        const int rawValue = static_cast<int>(axis);
        if (rawValue < 0 || rawValue >= static_cast<int>(InputCodeLimits::GamepadAxisCount))
            return GamepadAxes::Invalid;
        return GamepadAxisCode{ static_cast<int16_t>(rawValue) };
    }

    inline SDL_GamepadAxis ToSDLGamepadAxis(GamepadAxisCode axis)
    {
        const int16_t rawValue = axis.GetValue();
        if (rawValue < 0 || rawValue >= static_cast<int16_t>(SDL_GAMEPAD_AXIS_COUNT))
            return SDL_GAMEPAD_AXIS_INVALID;
        return static_cast<SDL_GamepadAxis>(rawValue);
    }
}
