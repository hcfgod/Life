#pragma once

#include "Core/Application.h"
#include "Core/Log.h"
#include "Platform/SDL/SDLEvent.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>

namespace Life
{
    struct ApplicationRunnerState
    {
        Scope<Application> ApplicationInstance;
        std::chrono::steady_clock::time_point LastFrameTime;
        std::mutex EventMutex;
        std::vector<Scope<Event>> PendingEvents;
    };

    inline ApplicationRunnerState* CreateApplicationRunner(ApplicationCommandLineArgs args, bool useExternalEventPump)
    {
        Log::Init();

        Scope<ApplicationRunnerState> state = CreateScope<ApplicationRunnerState>();
        state->ApplicationInstance = CreateApplication(args);
        state->ApplicationInstance->Initialize(useExternalEventPump);
        state->LastFrameTime = std::chrono::steady_clock::now();
        return state.release();
    }

    inline void QueueApplicationEvent(ApplicationRunnerState* state, Scope<Event> event)
    {
        if (state == nullptr || !event)
            return;

        std::scoped_lock lock(state->EventMutex);
        state->PendingEvents.emplace_back(std::move(event));
    }

    inline void QueueSDLEvent(ApplicationRunnerState* state, const SDL_Event& event)
    {
        QueueApplicationEvent(state, TranslateSDLEvent(event));
    }

    inline bool RunApplicationRunnerIteration(ApplicationRunnerState* state)
    {
        if (state == nullptr || state->ApplicationInstance == nullptr)
            return false;

        std::vector<Scope<Event>> pendingEvents;
        {
            std::scoped_lock lock(state->EventMutex);
            pendingEvents.swap(state->PendingEvents);
        }

        for (Scope<Event>& event : pendingEvents)
            state->ApplicationInstance->HandleEvent(*event);

        auto currentFrameTime = std::chrono::steady_clock::now();
        float timestep = std::chrono::duration<float>(currentFrameTime - state->LastFrameTime).count();
        state->LastFrameTime = currentFrameTime;

        state->ApplicationInstance->RunFrame(timestep);
        return state->ApplicationInstance->IsRunning();
    }

    inline void DestroyApplicationRunner(ApplicationRunnerState*& state)
    {
        if (state == nullptr)
            return;

        state->ApplicationInstance->Finalize();
        state->ApplicationInstance.reset();
        delete state;
        state = nullptr;
    }

    inline int HandleApplicationBootstrapException(const std::exception& exception)
    {
        LOG_CORE_ERROR("Application terminated with an exception: {}", exception.what());
        return 1;
    }

    inline SDL_AppResult HandleSDLApplicationBootstrapException(const std::exception& exception)
    {
        LOG_CORE_ERROR("Application terminated with an exception: {}", exception.what());
        return SDL_APP_FAILURE;
    }

    inline int RunApplicationMain(ApplicationCommandLineArgs args)
    {
        try
        {
            ApplicationRunnerState* state = CreateApplicationRunner(args, false);

            while (RunApplicationRunnerIteration(state))
            {
            }

            DestroyApplicationRunner(state);
            return 0;
        }
        catch (const std::exception& exception)
        {
            return HandleApplicationBootstrapException(exception);
        }
    }
}
