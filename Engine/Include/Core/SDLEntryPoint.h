#pragma once

#include "Core/ApplicationRunner.h"
#include "Core/Log.h"
#include "Platform/SDL/SDLEvent.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

namespace Life
{
    inline SDL_AppResult HandleSDLApplicationBootstrapException(const std::exception& exception)
    {
        LOG_CORE_ERROR("Application terminated with an exception: {}", exception.what());
        return SDL_APP_FAILURE;
    }
}

inline SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    try
    {
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
        return Life::HandleSDLApplicationBootstrapException(exception);
    }
}

inline SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    auto* state = static_cast<Life::ApplicationRunnerState*>(appstate);

    if (state == nullptr)
        return SDL_APP_FAILURE;

    try
    {
        Life::QueueApplicationEvent(state, Life::TranslateSDLEvent(*event));
        return state->ApplicationInstance->IsRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        return Life::HandleSDLApplicationBootstrapException(exception);
    }
}

inline void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    (void)result;

    auto* state = static_cast<Life::ApplicationRunnerState*>(appstate);
    Life::DestroyApplicationRunner(state);
}
