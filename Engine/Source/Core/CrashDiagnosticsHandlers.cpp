#include "CrashDiagnostics/CrashDiagnosticsHandlers.h"
#include "CrashDiagnostics/CrashDiagnosticsReportWriter.h"

#include <cstdlib>

namespace Life::CrashDiagnosticsDetail
{
    void HandleTerminate()
    {
        CrashDiagnosticsState& state = GetState();
        if (state.HandlingCrash.exchange(true))
            std::_Exit(EXIT_FAILURE);

        CrashDiagnosticsEvent event;
        event.Category = "terminate";
        event.Phase = "std::terminate";
        event.Reason = "std::terminate invoked";
        event.Details = "std::terminate was invoked without an active std::exception.";

        if (const std::exception_ptr exceptionPointer = std::current_exception())
        {
            try
            {
                std::rethrow_exception(exceptionPointer);
            }
            catch (const std::exception& exception)
            {
                event.Reason = exception.what();
                event.Details = BuildCrashExceptionDetails(exception);
            }
            catch (...)
            {
                event.Reason = "Unknown non-standard exception";
                event.Details = "std::terminate was invoked with a non-standard active exception.";
            }
        }

        event.StackTrace = CaptureCrashStackTrace(LoadConfigurationSnapshot()->Specification.MaxStackFrames);
        WriteCrashDiagnosticsReport(
            event
#if defined(LIFE_PLATFORM_WINDOWS)
            , nullptr
#endif
        );
        FlushCrashDiagnosticLoggers();
        std::_Exit(EXIT_FAILURE);
    }

#if defined(LIFE_PLATFORM_WINDOWS)
    LONG WINAPI HandleUnhandledStructuredException(EXCEPTION_POINTERS* exceptionPointers)
    {
        CrashDiagnosticsState& state = GetState();
        if (state.HandlingCrash.exchange(true))
            return EXCEPTION_EXECUTE_HANDLER;

        CrashDiagnosticsEvent event;
        event.Category = "structured-exception";
        event.Phase = "SetUnhandledExceptionFilter";
        event.WindowsExceptionCode = exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr
            ? exceptionPointers->ExceptionRecord->ExceptionCode
            : 0;
        event.Reason = event.WindowsExceptionCode != 0
            ? DescribeWindowsExceptionCode(event.WindowsExceptionCode)
            : "Unknown Windows exception";
        event.Details = "Unhandled structured exception captured by the engine crash filter.";
        if (exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr)
        {
            event.FaultAddress = reinterpret_cast<std::uintptr_t>(exceptionPointers->ExceptionRecord->ExceptionAddress);
        }

        event.StackTrace = CaptureCrashStackTrace(LoadConfigurationSnapshot()->Specification.MaxStackFrames);
        WriteCrashDiagnosticsReport(event, exceptionPointers);
        FlushCrashDiagnosticLoggers();
        return EXCEPTION_EXECUTE_HANDLER;
    }
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
    std::vector<int> GetCrashSignalNumbers()
    {
        std::vector<int> signalNumbers{ SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGTERM };
#ifdef SIGBUS
        signalNumbers.push_back(SIGBUS);
#endif
        return signalNumbers;
    }

    void HandleFatalSignal(int signalNumber, siginfo_t* signalInfo, void*)
    {
        CrashDiagnosticsState& state = GetState();
        if (state.HandlingCrash.exchange(true))
            std::_Exit(128 + signalNumber);

        CrashDiagnosticsEvent event;
        event.Category = "signal";
        event.Phase = "sigaction";
        event.SignalNumber = signalNumber;
        event.Reason = DescribeCrashSignal(signalNumber);
        event.Details = "Fatal signal captured by the engine crash handler.";
        if (signalInfo != nullptr)
            event.FaultAddress = reinterpret_cast<std::uintptr_t>(signalInfo->si_addr);

        event.StackTrace = CaptureCrashStackTrace(LoadConfigurationSnapshot()->Specification.MaxStackFrames);
        WriteCrashDiagnosticsReport(event);
        FlushCrashDiagnosticLoggers();

        std::signal(signalNumber, SIG_DFL);
        std::raise(signalNumber);
        std::_Exit(128 + signalNumber);
    }
#endif

    void ShutdownHandlersLocked(CrashDiagnosticsState& state)
    {
        if (!state.Installed)
            return;

        if (state.PreviousTerminateHandler != nullptr)
            std::set_terminate(state.PreviousTerminateHandler);
        state.PreviousTerminateHandler = nullptr;

#if defined(LIFE_PLATFORM_WINDOWS)
        ::SetUnhandledExceptionFilter(state.PreviousUnhandledExceptionFilter);
        state.PreviousUnhandledExceptionFilter = nullptr;
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
        for (const CrashDiagnosticsState::SignalRegistration& registration : state.SignalRegistrations)
            sigaction(registration.SignalNumber, &registration.PreviousAction, nullptr);
        state.SignalRegistrations.clear();
#endif

        state.Installed = false;
        state.HandlingCrash.store(false);
    }

    void InstallHandlersLocked(CrashDiagnosticsState& state, const CrashReportingSpecification& specification)
    {
        state.HandlingCrash.store(false);
        if (!specification.Enabled || !specification.InstallHandlers)
        {
            state.Installed = false;
            return;
        }

        bool installedAnyHandler = false;

        if (specification.CaptureTerminate)
        {
            state.PreviousTerminateHandler = std::get_terminate();
            std::set_terminate(&HandleTerminate);
            installedAnyHandler = true;
        }
        else
        {
            state.PreviousTerminateHandler = nullptr;
        }

#if defined(LIFE_PLATFORM_WINDOWS)
        if (specification.CaptureUnhandledExceptions)
        {
            state.PreviousUnhandledExceptionFilter = ::SetUnhandledExceptionFilter(&HandleUnhandledStructuredException);
            installedAnyHandler = true;
        }
        else
        {
            state.PreviousUnhandledExceptionFilter = nullptr;
        }
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
        state.SignalRegistrations.clear();
        if (specification.CaptureSignals)
        {
            struct sigaction action{};
            sigemptyset(&action.sa_mask);
            action.sa_flags = SA_SIGINFO | SA_RESETHAND;
            action.sa_sigaction = &HandleFatalSignal;

            for (const int signalNumber : GetCrashSignalNumbers())
            {
                struct sigaction previousAction{};
                if (sigaction(signalNumber, &action, &previousAction) == 0)
                {
                    state.SignalRegistrations.push_back({ signalNumber, previousAction });
                    installedAnyHandler = true;
                }
            }
        }
#endif

        state.Installed = installedAnyHandler;
    }
}
