#pragma once

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

        virtual const WindowSpecification& GetSpecification() const = 0;
        virtual void* GetNativeHandle() const = 0;

    protected:
        Window() = default;
    };
}
