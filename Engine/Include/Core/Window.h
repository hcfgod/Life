#pragma once

#include "Core/Events/Event.h"
#include "Core/Memory.h"

#include <cstdint>
#include <string>

namespace Life
{
    struct WindowSpecification
    {
        std::string Title = "Life Window";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool VSync = true;
    };

    class Window
    {
    public:
        virtual ~Window() = default;

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        virtual Scope<Event> PollEvent() = 0;
        virtual const WindowSpecification& GetSpecification() const = 0;
        virtual void* GetNativeHandle() const = 0;

    protected:
        Window() = default;
    };

    Scope<Window> CreatePlatformWindow(const WindowSpecification& specification);
}
