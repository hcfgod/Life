#pragma once

#include "Core/CrashDiagnostics.h"
#include "Core/Error.h"
#include "Core/ApplicationHost.h"
#include "Core/Log.h"

#include <chrono>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>

namespace Life
{
    inline std::vector<std::string> BuildCommandLineVector(ApplicationCommandLineArgs args)
    {
        std::vector<std::string> commandLine;
        commandLine.reserve(static_cast<std::size_t>(args.Count > 0 ? args.Count : 0));

        for (int index = 0; index < args.Count; ++index)
        {
            if (args[index] != nullptr)
                commandLine.emplace_back(args[index]);
        }

        return commandLine;
    }

    struct ApplicationRunnerState
    {
        Scope<ApplicationHost> Host;
        std::chrono::steady_clock::time_point LastFrameTime;
        std::mutex EventMutex;
        std::vector<Scope<Event>> PendingEvents;
        bool UseExternalEventPump = false;
    };

    inline void DispatchApplicationEvents(ApplicationHost& host, std::vector<Scope<Event>>& pendingEvents)
    {
        for (Scope<Event>& event : pendingEvents)
        {
            host.HandleEvent(*event);

            if (!host.IsRunning())
                break;
        }

        pendingEvents.clear();
    }

    inline void PollApplicationRuntimeEvents(ApplicationHost& host)
    {
        while (host.IsRunning())
        {
            Scope<Event> event = host.GetRuntime().PollEvent();
            if (!event)
                return;

            host.HandleEvent(*event);
        }
    }

    inline bool RunApplicationLoopIteration(
        ApplicationHost& host,
        std::chrono::steady_clock::time_point& lastFrameTime,
        std::vector<Scope<Event>>& pendingEvents,
        bool useExternalEventPump)
    {
        if (!host.IsRunning())
            return false;

        DispatchApplicationEvents(host, pendingEvents);

        if (host.IsRunning() && !useExternalEventPump)
            PollApplicationRuntimeEvents(host);

        if (!host.IsRunning())
            return false;

        auto currentFrameTime = std::chrono::steady_clock::now();
        float timestep = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
        lastFrameTime = currentFrameTime;

        host.RunFrame(timestep);
        return host.IsRunning();
    }

    inline void RunApplication(ApplicationHost& host)
    {
        host.Initialize();

        struct ApplicationFinalizer
        {
            ApplicationHost& Instance;

            ~ApplicationFinalizer()
            {
                Instance.Finalize();
            }
        } finalizer{ host };

        std::vector<Scope<Event>> pendingEvents;
        auto lastFrameTime = std::chrono::steady_clock::now();

        while (RunApplicationLoopIteration(host, lastFrameTime, pendingEvents, false))
        {
        }
    }

    inline ApplicationRunnerState* CreateApplicationRunner(ApplicationCommandLineArgs args, bool useExternalEventPump)
    {
        CrashDiagnostics::Install();
        CrashDiagnostics::SetApplicationInfo("Life Application", BuildCommandLineVector(args));

        Scope<ApplicationRunnerState> state = CreateScope<ApplicationRunnerState>();
        state->Host = CreateApplicationHost(args);
        state->UseExternalEventPump = useExternalEventPump;
        state->Host->Initialize();
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
        if (state == nullptr || state->Host == nullptr)
            return false;

        std::vector<Scope<Event>> pendingEvents;
        {
            std::scoped_lock lock(state->EventMutex);
            pendingEvents.swap(state->PendingEvents);
        }

        return RunApplicationLoopIteration(
            *state->Host,
            state->LastFrameTime,
            pendingEvents,
            state->UseExternalEventPump);
    }

    inline void DestroyApplicationRunner(ApplicationRunnerState*& state)
    {
        if (state == nullptr)
            return;

        state->Host->Finalize();
        state->Host.reset();
        delete state;
        state = nullptr;
    }

    inline int HandleApplicationBootstrapException(const std::exception& exception)
    {
        CrashDiagnostics::ReportHandledException(exception, "RunApplicationMain");

        if (const auto* error = dynamic_cast<const Error*>(&exception))
        {
            Error::LogError(*error);
            return 1;
        }

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
