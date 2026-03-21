#pragma once

#include "Core/Events/Event.h"

#include <cstdint>
#include <string>

namespace Life
{
    class WindowCloseEvent final : public Event
    {
    public:
        static EventType GetStaticType() { return EventType::WindowClose; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowCloseEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
    };

    class WindowResizeEvent final : public Event
    {
    public:
        WindowResizeEvent(uint32_t width, uint32_t height)
            : m_Width(width), m_Height(height)
        {
        }

        static EventType GetStaticType() { return EventType::WindowResize; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowResizeEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(m_Width) + ", " + std::to_string(m_Height); }

    private:
        uint32_t m_Width;
        uint32_t m_Height;
    };

    class WindowMovedEvent final : public Event
    {
    public:
        WindowMovedEvent(int32_t x, int32_t y)
            : m_X(x), m_Y(y)
        {
        }

        static EventType GetStaticType() { return EventType::WindowMoved; }

        int32_t GetX() const { return m_X; }
        int32_t GetY() const { return m_Y; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowMovedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
        std::string ToString() const override { return std::string(GetName()) + ": " + std::to_string(m_X) + ", " + std::to_string(m_Y); }

    private:
        int32_t m_X;
        int32_t m_Y;
    };

    class WindowFocusGainedEvent final : public Event
    {
    public:
        static EventType GetStaticType() { return EventType::WindowFocusGained; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowFocusGainedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
    };

    class WindowFocusLostEvent final : public Event
    {
    public:
        static EventType GetStaticType() { return EventType::WindowFocusLost; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowFocusLostEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
    };

    class WindowMinimizedEvent final : public Event
    {
    public:
        static EventType GetStaticType() { return EventType::WindowMinimized; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowMinimizedEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
    };

    class WindowRestoredEvent final : public Event
    {
    public:
        static EventType GetStaticType() { return EventType::WindowRestored; }

        EventType GetEventType() const override { return GetStaticType(); }
        const char* GetName() const override { return "WindowRestoredEvent"; }
        int GetCategoryFlags() const override { return static_cast<int>(EventCategory::Application) | static_cast<int>(EventCategory::Window); }
    };
}
