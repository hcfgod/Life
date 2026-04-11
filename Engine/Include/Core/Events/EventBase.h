#pragma once

#include <string>

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
        KeyPressed,
        KeyReleased,
        MouseMoved,
        MouseScrolled,
        MouseButtonPressed,
        MouseButtonReleased,
        GamepadAdded,
        GamepadRemoved,
        GamepadButtonPressed,
        GamepadButtonReleased,
        GamepadAxisMoved,
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
        MouseButton = 1 << 5,
        Gamepad = 1 << 6
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
}
