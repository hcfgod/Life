#include "Core/ApplicationRuntime.h"
#include "Core/Error.h"
#include "Core/Input/InputSystem.h"
#include "Core/Log.h"
#include "Core/ServiceRegistry.h"
#include "Graphics/ImGuiSystem.h"
#include "Platform/SDL/SDLEvent.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <mutex>
#include <utility>

namespace Life
{
    namespace
    {
        struct SDLRuntimeState final
        {
            std::mutex Mutex;
            std::size_t ReferenceCount = 0;
        };

        SDLRuntimeState& GetSDLRuntimeState()
        {
            static SDLRuntimeState state;
            return state;
        }

        void AcquireSDLVideoRuntime()
        {
            SDLRuntimeState& state = GetSDLRuntimeState();
            std::scoped_lock lock(state.Mutex);
            if (state.ReferenceCount == 0)
            {
                if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
                    throw Error(ErrorCode::PlatformInitializationFailed, SDL_GetError(), std::source_location::current(), ErrorSeverity::Critical);
            }

            ++state.ReferenceCount;
        }

        void ReleaseSDLVideoRuntime() noexcept
        {
            SDLRuntimeState& state = GetSDLRuntimeState();
            std::scoped_lock lock(state.Mutex);
            if (state.ReferenceCount == 0)
                return;

            --state.ReferenceCount;
            if (state.ReferenceCount == 0)
                SDL_Quit();
        }
    }

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
            SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;

#ifdef LIFE_GRAPHICS_VULKAN
            flags |= SDL_WINDOW_VULKAN;
#endif

            m_WindowHandle.reset(SDL_CreateWindow(
                m_Specification.Title.c_str(),
                static_cast<int>(m_Specification.Width),
                static_cast<int>(m_Specification.Height),
                flags
            ));

            if (!m_WindowHandle)
            {
                throw Error(ErrorCode::WindowCreationFailed, SDL_GetError(), std::source_location::current(), ErrorSeverity::Critical);
            }

            LOG_CORE_INFO("Created window '{}'", m_Specification.Title);
        }

        ~SDLWindow() override
        {
            m_WindowHandle.reset();

            try
            {
                LOG_CORE_INFO("Destroyed window '{}'", m_Specification.Title);
            }
            catch (...)
            {
                std::fprintf(stderr, "Destroyed window '%s'\n", m_Specification.Title.c_str());
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

    class SDLApplicationRuntime final : public ApplicationRuntime
    {
    public:
        SDLApplicationRuntime()
        {
            AcquireSDLVideoRuntime();
        }

        ~SDLApplicationRuntime() override
        {
            ReleaseSDLVideoRuntime();
        }

        Scope<Window> CreatePlatformWindow(const WindowSpecification& specification) override
        {
            return CreateScope<SDLWindow>(specification);
        }

        Scope<Event> PollEvent() override
        {
            for (;;)
            {
                SDL_Event sdlEvent;
                if (!SDL_PollEvent(&sdlEvent))
                    return nullptr;

                if (ImGuiSystem* imguiSystem = GetServices().TryGet<ImGuiSystem>())
                    imguiSystem->OnSdlEvent(sdlEvent);

                if (InputSystem* inputSystem = GetServices().TryGet<InputSystem>())
                    inputSystem->OnSdlEvent(sdlEvent);

                if (Scope<Event> event = TranslateSDLEvent(sdlEvent))
                    return event;
            }
        }
    };

    Scope<ApplicationRuntime> CreatePlatformApplicationRuntime()
    {
        return CreateScope<SDLApplicationRuntime>();
    }
}
