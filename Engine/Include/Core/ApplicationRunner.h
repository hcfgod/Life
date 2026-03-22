#pragma once

#include "Core/Application.h"
#include "Core/Log.h"

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
        bool UseExternalEventPump = false;
    };

    inline void DispatchApplicationEvents(Application& application, std::vector<Scope<Event>>& pendingEvents)
    {
        for (Scope<Event>& event : pendingEvents)
        {
            application.HandleEvent(*event);

            if (!application.IsRunning())
                break;
        }

        pendingEvents.clear();
    }

    inline void PollApplicationRuntimeEvents(Application& application)
    {
        while (application.IsRunning())
        {
            Scope<Event> event = application.GetRuntime().PollEvent();
            if (!event)
                return;

            application.HandleEvent(*event);
        }
    }

    inline bool RunApplicationLoopIteration(
        Application& application,
        std::chrono::steady_clock::time_point& lastFrameTime,
        std::vector<Scope<Event>>& pendingEvents,
        bool useExternalEventPump)
    {
        if (!application.IsRunning())
            return false;

        DispatchApplicationEvents(application, pendingEvents);

        if (application.IsRunning() && !useExternalEventPump)
            PollApplicationRuntimeEvents(application);

        if (!application.IsRunning())
            return false;

        auto currentFrameTime = std::chrono::steady_clock::now();
        float timestep = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
        lastFrameTime = currentFrameTime;

        application.RunFrame(timestep);
        return application.IsRunning();
    }

    inline void RunApplication(Application& application)
    {
        Log::Init();
        application.Initialize();

        struct ApplicationFinalizer
        {
            Application& Instance;

            ~ApplicationFinalizer()
            {
                Instance.Finalize();
            }
        } finalizer{ application };

        std::vector<Scope<Event>> pendingEvents;
        auto lastFrameTime = std::chrono::steady_clock::now();

        while (RunApplicationLoopIteration(application, lastFrameTime, pendingEvents, false))
        {
        }
    }

    inline ApplicationRunnerState* CreateApplicationRunner(ApplicationCommandLineArgs args, bool useExternalEventPump)
    {
        Log::Init();

        Scope<ApplicationRunnerState> state = CreateScope<ApplicationRunnerState>();
        state->ApplicationInstance = CreateApplication(args);
        state->UseExternalEventPump = useExternalEventPump;
        state->ApplicationInstance->Initialize();
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

    inline bool RunApplicationRunnerIteration(ApplicationRunnerState* state)
    {
        if (state == nullptr || state->ApplicationInstance == nullptr)
            return false;

        std::vector<Scope<Event>> pendingEvents;
        {
            std::scoped_lock lock(state->EventMutex);
            pendingEvents.swap(state->PendingEvents);
        }

        return RunApplicationLoopIteration(
            *state->ApplicationInstance,
            state->LastFrameTime,
            pendingEvents,
            state->UseExternalEventPump);
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

    inline int RunApplicationMain(ApplicationCommandLineArgs args)
    {
        try
        {
            ApplicationRunnerState* state = CreateApplicationRunner(args, false);

            struct ApplicationRunnerDestroyer
            {
                ApplicationRunnerState*& Instance;

                ~ApplicationRunnerDestroyer()
                {
                    DestroyApplicationRunner(Instance);
                }
            } destroyer{ state };

            while (RunApplicationRunnerIteration(state))
            {
            }

            return 0;
        }
        catch (const std::exception& exception)
        {
            return HandleApplicationBootstrapException(exception);
        }
    }
}
