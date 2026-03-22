#include "Core/Window.h"
#include "Core/Log.h"
#include "Platform/SDL/SDLEvent.h"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace Life
{
    struct SDLWindowDeleter
    {
        void operator()(SDL_Window* window) const
        {
            if (window != nullptr)
                SDL_DestroyWindow(window);
        }
    };

    class SDLWindow final : public Window
    {
    public:
        explicit SDLWindow(WindowSpecification specification)
            : m_Specification(std::move(specification))
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
                throw std::runtime_error(SDL_GetError());

            m_WindowHandle.reset(SDL_CreateWindow(
                m_Specification.Title.c_str(),
                static_cast<int>(m_Specification.Width),
                static_cast<int>(m_Specification.Height),
                SDL_WINDOW_RESIZABLE
            ));

            if (!m_WindowHandle)
            {
                std::string error = SDL_GetError();
                SDL_Quit();
                throw std::runtime_error(error);
            }

            LOG_CORE_INFO("Created window '{}'", m_Specification.Title);
        }

        ~SDLWindow() override
        {
            m_WindowHandle.reset();
            SDL_Quit();

            try
            {
                LOG_CORE_INFO("Destroyed window '{}'", m_Specification.Title);
            }
            catch (...)
            {
            }
        }

        Scope<Event> PollEvent() override
        {
            for (;;)
            {
                SDL_Event sdlEvent;
                if (!SDL_PollEvent(&sdlEvent))
                    return nullptr;

                if (Scope<Event> event = TranslateSDLEvent(sdlEvent))
                    return event;
            }
        }

        const WindowSpecification& GetSpecification() const override
        {
            return m_Specification;
        }

        void* GetNativeHandle() const override
        {
            return m_WindowHandle.get();
        }

    private:
        WindowSpecification m_Specification;
        Scope<SDL_Window, SDLWindowDeleter> m_WindowHandle;
    };

    Scope<Window> CreatePlatformWindow(const WindowSpecification& specification)
    {
        return CreateScope<SDLWindow>(specification);
    }
}
