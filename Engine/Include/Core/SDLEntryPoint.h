#pragma once

#include "Core/ApplicationRunner.h"
#include "Core/Log.h"
#include "Platform/SDL/SDLEvent.h"
#include "Core/Error.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

namespace Life
{
    inline SDL_AppResult HandleSDLApplicationBootstrapException(const std::exception& exception)
    {
        CrashDiagnostics::ReportHandledException(exception, "SDL_AppInit");

        if (const auto* error = dynamic_cast<const Error*>(&exception))
        {
            Error::LogError(*error);
            return SDL_APP_FAILURE;
        }

        LOG_CORE_ERROR("SDL application bootstrap terminated with an exception: {}", exception.what());
        return SDL_APP_FAILURE;
    }

    inline SDL_AppResult HandleSDLApplicationRuntimeException(const std::exception& exception, std::string_view phase)
    {
        CrashDiagnostics::ReportHandledException(exception, phase);

        if (const auto* error = dynamic_cast<const Error*>(&exception))
        {
            Error::LogError(*error);
            return SDL_APP_FAILURE;
        }

        LOG_CORE_ERROR("SDL application runtime terminated with an exception during {}: {}", phase, exception.what());
        return SDL_APP_FAILURE;
    }
}

inline SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    try
    {
        Life::PrepareApplicationBootstrapDiagnostics({ argc, argv });
        Life::ApplicationRunnerState* state = Life::CreateApplicationRunner({ argc, argv }, true);
        state->LastFrameTime = std::chrono::steady_clock::now();
        *appstate = state;
        return SDL_APP_CONTINUE;
    }
    catch (const std::exception& exception)
    {
        *appstate = nullptr;
        return Life::HandleSDLApplicationBootstrapException(exception);
    }
}

inline SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto* state = static_cast<Life::ApplicationRunnerState*>(appstate);

    if (state == nullptr)
        return SDL_APP_FAILURE;

    try
    {
        return Life::RunApplicationRunnerIteration(state) ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        return Life::HandleSDLApplicationRuntimeException(exception, "SDL_AppIterate");
    }
}

inline SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    auto* state = static_cast<Life::ApplicationRunnerState*>(appstate);

    if (state == nullptr)
        return SDL_APP_FAILURE;

    try
    {
        SDL_Event queuedEvent = *event;
        Life::QueueApplicationEvent(
            state,
            Life::TranslateSDLEvent(queuedEvent),
            [queuedEvent](Life::ApplicationHost& host) mutable
            {
                host.GetInputSystem().OnSdlEvent(queuedEvent);
            });
        return state->Host->IsRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        return Life::HandleSDLApplicationRuntimeException(exception, "SDL_AppEvent");
    }
}

inline void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    (void)result;

    auto* state = static_cast<Life::ApplicationRunnerState*>(appstate);
    Life::DestroyApplicationRunner(state);
}
