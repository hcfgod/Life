#pragma once

#include "Core/Events/Event.h"
#include "Core/Input/InputCodes.h"

#include <cstdint>
#include <string>

namespace Life
{
    class KeyPressedEvent final : public Event
    {
    public:
        KeyPressedEvent(KeyCode keyCode, bool repeat)
            : m_KeyCode(keyCode), m_IsRepeat(repeat)
        {
        }

        static EventType GetStaticType() { return EventType::KeyPressed; }

        KeyCode GetKeyCode() const { return m_KeyCode; }
        bool IsRepeat() const { return m_IsRepeat; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "KeyPressedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Keyboard); }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_KeyCode.GetValue())); }

    private:
        KeyCode m_KeyCode = KeyCodes::Unknown;
        bool m_IsRepeat = false;
    };

    class KeyReleasedEvent final : public Event
    {
    public:
        explicit KeyReleasedEvent(KeyCode keyCode)
            : m_KeyCode(keyCode)
        {
        }

        static EventType GetStaticType() { return EventType::KeyReleased; }

        KeyCode GetKeyCode() const { return m_KeyCode; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "KeyReleasedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Keyboard); }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_KeyCode.GetValue())); }

    private:
        KeyCode m_KeyCode = KeyCodes::Unknown;
    };

    class MouseMovedEvent final : public Event
    {
    public:
        MouseMovedEvent(float x, float y, float deltaX, float deltaY)
            : m_X(x), m_Y(y), m_DeltaX(deltaX), m_DeltaY(deltaY)
        {
        }

        static EventType GetStaticType() { return EventType::MouseMoved; }

        float GetX() const { return m_X; }
        float GetY() const { return m_Y; }
        float GetDeltaX() const { return m_DeltaX; }
        float GetDeltaY() const { return m_DeltaY; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseMovedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Mouse); }
        std::string ToString() const override
        {
            return std::string(GetName()) + ": " + std::to_string(m_X) + ", " + std::to_string(m_Y);
        }

    private:
        float m_X = 0.0f;
        float m_Y = 0.0f;
        float m_DeltaX = 0.0f;
        float m_DeltaY = 0.0f;
    };

    class MouseScrolledEvent final : public Event
    {
    public:
        MouseScrolledEvent(float offsetX, float offsetY)
            : m_OffsetX(offsetX), m_OffsetY(offsetY)
        {
        }

        static EventType GetStaticType() { return EventType::MouseScrolled; }

        float GetOffsetX() const { return m_OffsetX; }
        float GetOffsetY() const { return m_OffsetY; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseScrolledEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Mouse); }
        std::string ToString() const override
        {
            return std::string(GetName()) + ": " + std::to_string(m_OffsetX) + ", " + std::to_string(m_OffsetY);
        }

    private:
        float m_OffsetX = 0.0f;
        float m_OffsetY = 0.0f;
    };

    class MouseButtonPressedEvent final : public Event
    {
    public:
        explicit MouseButtonPressedEvent(MouseButtonCode button)
            : m_Button(button)
        {
        }

        static EventType GetStaticType() { return EventType::MouseButtonPressed; }

        MouseButtonCode GetButton() const { return m_Button; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseButtonPressedEvent"; }
        int GetCategoryFlags() const override
        {
            return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Mouse) | static_cast<int>(EventCategory::MouseButton);
        }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_Button.GetValue())); }

    private:
        MouseButtonCode m_Button = MouseButtons::None;
    };

    class MouseButtonReleasedEvent final : public Event
    {
    public:
        explicit MouseButtonReleasedEvent(MouseButtonCode button)
            : m_Button(button)
        {
        }

        static EventType GetStaticType() { return EventType::MouseButtonReleased; }

        MouseButtonCode GetButton() const { return m_Button; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "MouseButtonReleasedEvent"; }
        int GetCategoryFlags() const override
        {
            return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Mouse) | static_cast<int>(EventCategory::MouseButton);
        }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_Button.GetValue())); }

    private:
        MouseButtonCode m_Button = MouseButtons::None;
    };

    class GamepadAddedEvent final : public Event
    {
    public:
        explicit GamepadAddedEvent(GamepadId gamepadId)
            : m_GamepadId(gamepadId)
        {
        }

        static EventType GetStaticType() { return EventType::GamepadAdded; }

        GamepadId GetGamepadId() const { return m_GamepadId; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "GamepadAddedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Gamepad); }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_GamepadId)); }

    private:
        GamepadId m_GamepadId = 0;
    };

    class GamepadRemovedEvent final : public Event
    {
    public:
        explicit GamepadRemovedEvent(GamepadId gamepadId)
            : m_GamepadId(gamepadId)
        {
        }

        static EventType GetStaticType() { return EventType::GamepadRemoved; }

        GamepadId GetGamepadId() const { return m_GamepadId; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "GamepadRemovedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Gamepad); }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(static_cast<int>(m_GamepadId)); }

    private:
        GamepadId m_GamepadId = 0;
    };

    class GamepadButtonPressedEvent final : public Event
    {
    public:
        GamepadButtonPressedEvent(GamepadId gamepadId, GamepadButtonCode button)
            : m_GamepadId(gamepadId), m_Button(button)
        {
        }

        static EventType GetStaticType() { return EventType::GamepadButtonPressed; }

        GamepadId GetGamepadId() const { return m_GamepadId; }
        GamepadButtonCode GetButton() const { return m_Button; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "GamepadButtonPressedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Gamepad); }
        std::string ToString() const override
        {
            return std::string(GetName()) + ": gamepad=" + std::to_string(static_cast<int>(m_GamepadId)) + ", button=" + std::to_string(static_cast<int>(m_Button.GetValue()));
        }

    private:
        GamepadId m_GamepadId = 0;
        GamepadButtonCode m_Button = GamepadButtons::Invalid;
    };

    class GamepadButtonReleasedEvent final : public Event
    {
    public:
        GamepadButtonReleasedEvent(GamepadId gamepadId, GamepadButtonCode button)
            : m_GamepadId(gamepadId), m_Button(button)
        {
        }

        static EventType GetStaticType() { return EventType::GamepadButtonReleased; }

        GamepadId GetGamepadId() const { return m_GamepadId; }
        GamepadButtonCode GetButton() const { return m_Button; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "GamepadButtonReleasedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Gamepad); }
        std::string ToString() const override
        {
            return std::string(GetName()) + ": gamepad=" + std::to_string(static_cast<int>(m_GamepadId)) + ", button=" + std::to_string(static_cast<int>(m_Button.GetValue()));
        }

    private:
        GamepadId m_GamepadId = 0;
        GamepadButtonCode m_Button = GamepadButtons::Invalid;
    };

    class GamepadAxisMovedEvent final : public Event
    {
    public:
        GamepadAxisMovedEvent(GamepadId gamepadId, GamepadAxisCode axis, int16_t value)
            : m_GamepadId(gamepadId), m_Axis(axis), m_Value(value)
        {
        }

        static EventType GetStaticType() { return EventType::GamepadAxisMoved; }

        GamepadId GetGamepadId() const { return m_GamepadId; }
        GamepadAxisCode GetAxis() const { return m_Axis; }
        int16_t GetValue() const { return m_Value; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "GamepadAxisMovedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Input) | static_cast<int>(EventCategory::Gamepad); }
        std::string ToString() const override
        {
            return std::string(GetName()) + ": gamepad=" + std::to_string(static_cast<int>(m_GamepadId)) + ", axis=" + std::to_string(static_cast<int>(m_Axis.GetValue())) + ", value=" + std::to_string(static_cast<int>(m_Value));
        }

    private:
        GamepadId m_GamepadId = 0;
        GamepadAxisCode m_Axis = GamepadAxes::Invalid;
        int16_t m_Value = 0;
    };
}
