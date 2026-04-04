#pragma once

#include "Core/CrashDiagnostics.h"
#include "Core/Error.h"
#include "Core/ApplicationHost.h"
#include "Core/Log.h"

#include <chrono>
#include <exception>
#include <functional>
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

    inline void PrepareApplicationBootstrapDiagnostics(
        ApplicationCommandLineArgs args,
        std::string applicationName = "Life Application")
    {
        CrashDiagnostics::Install();
        CrashDiagnostics::SetApplicationInfo(
            applicationName.empty() ? "Life Application" : std::move(applicationName),
            BuildCommandLineVector(args));
    }

    inline void ReportApplicationRunnerTeardownException(const std::exception& exception) noexcept
    {
        try
        {
            CrashDiagnostics::ReportHandledException(exception, "ApplicationRunnerTeardown");
        }
        catch (...)
        {
        }

        try
        {
            LOG_CORE_ERROR("Application runner teardown suppressed an exception: {}", exception.what());
        }
        catch (...)
        {
        }
    }

    inline void ReportApplicationRunnerTeardownException() noexcept
    {
        try
        {
            LOG_CORE_ERROR("Application runner teardown suppressed a non-standard exception.");
        }
        catch (...)
        {
        }
    }

    struct QueuedApplicationEvent
    {
        std::function<void(ApplicationHost&)> Prepare;
        Scope<Event> Event;
    };

    struct ApplicationRunnerState
    {
        Scope<ApplicationHost> Host;
        std::chrono::steady_clock::time_point LastFrameTime;
        std::mutex EventMutex;
        std::vector<QueuedApplicationEvent> PendingEvents;
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

    inline void DispatchApplicationEvents(ApplicationHost& host, std::vector<QueuedApplicationEvent>& pendingEvents)
    {
        for (QueuedApplicationEvent& pendingEvent : pendingEvents)
        {
            if (pendingEvent.Prepare)
                pendingEvent.Prepare(host);

            if (!host.IsRunning())
                break;

            if (pendingEvent.Event)
                host.HandleEvent(*pendingEvent.Event);

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

    inline bool RunApplicationLoopIteration(
        ApplicationHost& host,
        std::chrono::steady_clock::time_point& lastFrameTime,
        std::vector<QueuedApplicationEvent>& pendingEvents,
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
                try
                {
                    Instance.Finalize();
                }
                catch (const std::exception& exception)
                {
                    ReportApplicationRunnerTeardownException(exception);
                }
                catch (...)
                {
                    ReportApplicationRunnerTeardownException();
                }
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
        state->PendingEvents.emplace_back(QueuedApplicationEvent{ {}, std::move(event) });
    }

    inline void QueueApplicationEvent(
        ApplicationRunnerState* state,
        Scope<Event> event,
        std::function<void(ApplicationHost&)> prepareCallback)
    {
        if (state == nullptr || (!event && !prepareCallback))
            return;

        std::scoped_lock lock(state->EventMutex);
        state->PendingEvents.emplace_back(QueuedApplicationEvent{ std::move(prepareCallback), std::move(event) });
    }

    inline bool RunApplicationRunnerIteration(ApplicationRunnerState* state)
    {
        if (state == nullptr || state->Host == nullptr)
            return false;

        std::vector<QueuedApplicationEvent> pendingEvents;
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

        try
        {
            state->Host->Finalize();
        }
        catch (const std::exception& exception)
        {
            ReportApplicationRunnerTeardownException(exception);
        }
        catch (...)
        {
            ReportApplicationRunnerTeardownException();
        }

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

    inline int HandleApplicationRuntimeException(const std::exception& exception, std::string_view phase = "RunApplicationLoop")
    {
        CrashDiagnostics::ReportHandledException(exception, phase);

        if (const auto* error = dynamic_cast<const Error*>(&exception))
        {
            Error::LogError(*error);
            return 1;
        }

        LOG_CORE_ERROR("Application runtime terminated with an exception during {}: {}", phase, exception.what());
        return 1;
    }

    inline int RunApplicationMain(ApplicationCommandLineArgs args)
    {
        try
        {
            PrepareApplicationBootstrapDiagnostics(args);
            ApplicationRunnerState* state = CreateApplicationRunner(args, false);

            struct ApplicationRunnerDestroyer
            {
                ApplicationRunnerState*& Instance;

                ~ApplicationRunnerDestroyer()
                {
                    DestroyApplicationRunner(Instance);
                }
            } destroyer{ state };

            try
            {
                while (RunApplicationRunnerIteration(state))
                {
                }
            }
            catch (const std::exception& exception)
            {
                return HandleApplicationRuntimeException(exception);
            }

            return 0;
        }
        catch (const std::exception& exception)
        {
            return HandleApplicationBootstrapException(exception);
        }
    }
}
