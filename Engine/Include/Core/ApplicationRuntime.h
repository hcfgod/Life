#pragma once

#include "Core/Events/Event.h"
#include "Core/Memory.h"
#include "Core/Window.h"

namespace Life
{
    class ApplicationRuntime
    {
    public:
        virtual ~ApplicationRuntime() = default;

        virtual Scope<Window> CreatePlatformWindow(const WindowSpecification& specification) = 0;
        virtual Scope<Event> PollEvent() = 0;

    protected:
        ApplicationRuntime() = default;
    };

    Scope<ApplicationRuntime> CreatePlatformApplicationRuntime();
}
