#include "Core/CrashDiagnostics.h"

#include "Core/Error.h"
#include "Core/Log.h"
#include "Platform/Platform.h"

#include <atomic>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <vector>

#if defined(LIFE_PLATFORM_WINDOWS)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
    #include <DbgHelp.h>
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
    #include <execinfo.h>
    #include <unistd.h>
#endif

namespace Life
{
    namespace
    {
        struct CrashConfigurationSnapshot
        {
            CrashReportingSpecification Specification;
            std::string ApplicationName = "Life Application";
            std::vector<std::string> CommandLine;
        };

        struct CrashEvent
        {
            std::string Category;
            std::string Reason;
            std::string Phase;
            std::string Details;
            std::vector<std::string> StackTrace;
            int SignalNumber = 0;
            std::uintptr_t FaultAddress = 0;
            std::uint32_t WindowsExceptionCode = 0;
        };

        struct CrashDiagnosticsState
        {
            std::mutex Mutex;
            std::shared_ptr<CrashConfigurationSnapshot> Snapshot = std::make_shared<CrashConfigurationSnapshot>();
            std::filesystem::path LastReportPath;
            bool Installed = false;
            std::terminate_handler PreviousTerminateHandler = nullptr;
            std::atomic<bool> HandlingCrash = false;
#if defined(LIFE_PLATFORM_WINDOWS)
            LPTOP_LEVEL_EXCEPTION_FILTER PreviousUnhandledExceptionFilter = nullptr;
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
            struct SignalRegistration
            {
                int SignalNumber = 0;
                struct sigaction PreviousAction{};
            };

            std::vector<SignalRegistration> SignalRegistrations;
#endif
        };

        CrashDiagnosticsState& GetState()
        {
            static CrashDiagnosticsState state;
            return state;
        }

        std::shared_ptr<CrashConfigurationSnapshot> LoadSnapshot()
        {
            return std::atomic_load(&GetState().Snapshot);
        }

        void StoreSnapshot(std::shared_ptr<CrashConfigurationSnapshot> snapshot)
        {
            std::atomic_store(&GetState().Snapshot, std::move(snapshot));
        }

        void StoreLastReportPath(const std::filesystem::path& reportPath)
        {
            CrashDiagnosticsState& state = GetState();
            std::scoped_lock lock(state.Mutex);
            state.LastReportPath = reportPath;
        }

        std::string SanitizeFileComponent(std::string_view value)
        {
            std::string sanitized;
            sanitized.reserve(value.size());

            for (const char character : value)
            {
                const unsigned char unsignedCharacter = static_cast<unsigned char>(character);
                if (std::isalnum(unsignedCharacter) || character == '-' || character == '_' || character == '.')
                {
                    sanitized.push_back(character);
                }
                else
                {
                    sanitized.push_back('_');
                }
            }

            if (sanitized.empty())
                return "LifeApplication";

            return sanitized;
        }

        std::tm GetLocalTime(std::time_t timeValue)
        {
            std::tm localTime{};
#if defined(LIFE_PLATFORM_WINDOWS)
            localtime_s(&localTime, &timeValue);
#else
            localtime_r(&timeValue, &localTime);
#endif
            return localTime;
        }

        std::string FormatCurrentTimestamp(const char* format)
        {
            const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            const std::tm localTime = GetLocalTime(now);
            std::ostringstream stream;
            stream << std::put_time(&localTime, format);
            return stream.str();
        }

        std::string GetCurrentTimestampDisplayString()
        {
            return FormatCurrentTimestamp("%Y-%m-%d %H:%M:%S");
        }

        std::string GetCurrentTimestampToken()
        {
            return FormatCurrentTimestamp("%Y%m%d-%H%M%S");
        }

        template<typename TValue>
        std::string ToHexString(TValue value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase << static_cast<unsigned long long>(value);
            return stream.str();
        }

        std::string QuoteCommandLineArgument(std::string_view argument)
        {
            if (argument.find_first_of(" \t\"") == std::string_view::npos)
                return std::string(argument);

            std::string quoted = "\"";
            for (const char character : argument)
            {
                if (character == '\"')
                    quoted += '\\';
                quoted += character;
            }
            quoted += '"';
            return quoted;
        }

        std::string JoinCommandLine(const std::vector<std::string>& commandLine)
        {
            std::ostringstream stream;
            for (std::size_t index = 0; index < commandLine.size(); ++index)
            {
                if (index > 0)
                    stream << ' ';
                stream << QuoteCommandLineArgument(commandLine[index]);
            }
            return stream.str();
        }

        std::string GetThreadIdString()
        {
            std::ostringstream stream;
            stream << std::this_thread::get_id();
            return stream.str();
        }

        void ReportSuppressedException(std::string_view operation) noexcept
        {
            constexpr std::string_view prefix = "Life::CrashDiagnostics suppressed an exception while ";
            constexpr std::string_view suffix = ".\n";
            (void)std::fwrite(prefix.data(), sizeof(char), prefix.size(), stderr);
            (void)std::fwrite(operation.data(), sizeof(char), operation.size(), stderr);
            (void)std::fwrite(suffix.data(), sizeof(char), suffix.size(), stderr);
            std::fflush(stderr);
        }

        void ReportSuppressedException(std::string_view operation, std::string_view details) noexcept
        {
            constexpr std::string_view prefix = "Life::CrashDiagnostics suppressed an exception while ";
            constexpr std::string_view separator = ": ";
            constexpr std::string_view suffix = ".\n";
            (void)std::fwrite(prefix.data(), sizeof(char), prefix.size(), stderr);
            (void)std::fwrite(operation.data(), sizeof(char), operation.size(), stderr);
            if (!details.empty())
            {
                (void)std::fwrite(separator.data(), sizeof(char), separator.size(), stderr);
                (void)std::fwrite(details.data(), sizeof(char), details.size(), stderr);
            }
            (void)std::fwrite(suffix.data(), sizeof(char), suffix.size(), stderr);
            std::fflush(stderr);
        }

        void FlushLogs()
        {
            try
            {
                if (const std::shared_ptr<spdlog::logger> coreLogger = Log::GetCoreLogger())
                    coreLogger->flush();
            }
            catch (const std::exception& exception)
            {
                ReportSuppressedException("flushing the core logger", exception.what());
            }
            catch (...)
            {
                ReportSuppressedException("flushing the core logger");
            }

            try
            {
                if (const std::shared_ptr<spdlog::logger> clientLogger = Log::GetClientLogger())
                    clientLogger->flush();
            }
            catch (const std::exception& exception)
            {
                ReportSuppressedException("flushing the client logger", exception.what());
            }
            catch (...)
            {
                ReportSuppressedException("flushing the client logger");
            }
        }

        std::string SignalName(int signalNumber)
        {
            switch (signalNumber)
            {
            case SIGABRT:
                return "SIGABRT";
            case SIGFPE:
                return "SIGFPE";
            case SIGILL:
                return "SIGILL";
            case SIGSEGV:
                return "SIGSEGV";
            case SIGTERM:
                return "SIGTERM";
#ifdef SIGBUS
            case SIGBUS:
                return "SIGBUS";
#endif
            default:
                return "UNKNOWN_SIGNAL";
            }
        }

#if defined(LIFE_PLATFORM_WINDOWS)
        std::string WindowsExceptionCodeToString(std::uint32_t exceptionCode)
        {
            switch (exceptionCode)
            {
            case EXCEPTION_ACCESS_VIOLATION:
                return "EXCEPTION_ACCESS_VIOLATION";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
            case EXCEPTION_BREAKPOINT:
                return "EXCEPTION_BREAKPOINT";
            case EXCEPTION_DATATYPE_MISALIGNMENT:
                return "EXCEPTION_DATATYPE_MISALIGNMENT";
            case EXCEPTION_FLT_DENORMAL_OPERAND:
                return "EXCEPTION_FLT_DENORMAL_OPERAND";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_INEXACT_RESULT:
                return "EXCEPTION_FLT_INEXACT_RESULT";
            case EXCEPTION_FLT_INVALID_OPERATION:
                return "EXCEPTION_FLT_INVALID_OPERATION";
            case EXCEPTION_FLT_OVERFLOW:
                return "EXCEPTION_FLT_OVERFLOW";
            case EXCEPTION_FLT_STACK_CHECK:
                return "EXCEPTION_FLT_STACK_CHECK";
            case EXCEPTION_FLT_UNDERFLOW:
                return "EXCEPTION_FLT_UNDERFLOW";
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                return "EXCEPTION_ILLEGAL_INSTRUCTION";
            case EXCEPTION_IN_PAGE_ERROR:
                return "EXCEPTION_IN_PAGE_ERROR";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                return "EXCEPTION_INT_DIVIDE_BY_ZERO";
            case EXCEPTION_INT_OVERFLOW:
                return "EXCEPTION_INT_OVERFLOW";
            case EXCEPTION_INVALID_DISPOSITION:
                return "EXCEPTION_INVALID_DISPOSITION";
            case EXCEPTION_NONCONTINUABLE_EXCEPTION:
                return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
            case EXCEPTION_PRIV_INSTRUCTION:
                return "EXCEPTION_PRIV_INSTRUCTION";
            case EXCEPTION_STACK_OVERFLOW:
                return "EXCEPTION_STACK_OVERFLOW";
            default:
                return "UNKNOWN_WINDOWS_EXCEPTION";
            }
        }
#endif

        std::string BuildExceptionDetails(const std::exception& exception)
        {
            if (const auto* error = dynamic_cast<const Error*>(&exception))
                return error->ToDetailedString();

            return exception.what();
        }

        void EnsurePlatformMetadataAvailable()
        {
            if (PlatformDetection::IsInitialized())
                return;

            try
            {
                PlatformDetection::Initialize();
            }
            catch (const std::exception& exception)
            {
                ReportSuppressedException("initializing platform metadata", exception.what());
            }
            catch (...)
            {
                ReportSuppressedException("initializing platform metadata");
            }
        }

#if defined(LIFE_PLATFORM_WINDOWS)
        std::vector<std::string> CaptureStackTrace(std::size_t maxFrames)
        {
            if (maxFrames == 0)
                return {};

            const std::size_t boundedFrameCount = std::min<std::size_t>(maxFrames, static_cast<std::size_t>(std::numeric_limits<USHORT>::max()));
            std::vector<void*> frameAddresses(boundedFrameCount);
            const USHORT capturedFrameCount = ::CaptureStackBackTrace(
                0,
                static_cast<ULONG>(boundedFrameCount),
                frameAddresses.data(),
                nullptr);

            if (capturedFrameCount == 0)
                return {};

            std::vector<std::string> frames;
            frames.reserve(capturedFrameCount);

            const HANDLE processHandle = ::GetCurrentProcess();
            ::SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
            ::SymInitialize(processHandle, nullptr, TRUE);

            for (USHORT frameIndex = 0; frameIndex < capturedFrameCount; ++frameIndex)
            {
                const DWORD64 address = reinterpret_cast<DWORD64>(frameAddresses[frameIndex]);
                alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
                auto* symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
                symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbolInfo->MaxNameLen = MAX_SYM_NAME;

                DWORD64 displacement = 0;
                std::ostringstream frameStream;
                frameStream << '[' << frameIndex << "] " << ToHexString(address);

                if (::SymFromAddr(processHandle, address, &displacement, symbolInfo) != FALSE)
                {
                    frameStream << ' ' << symbolInfo->Name;
                    if (displacement != 0)
                        frameStream << " +" << ToHexString(displacement);
                }

                IMAGEHLP_LINE64 lineInformation{};
                lineInformation.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                DWORD lineDisplacement = 0;
                if (::SymGetLineFromAddr64(processHandle, address, &lineDisplacement, &lineInformation) != FALSE)
                {
                    frameStream << " (" << lineInformation.FileName << ':' << lineInformation.LineNumber << ')';
                }

                frames.emplace_back(frameStream.str());
            }

            ::SymCleanup(processHandle);

            return frames;
        }
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
        std::vector<std::string> CaptureStackTrace(std::size_t maxFrames)
        {
            if (maxFrames == 0)
                return {};

            std::vector<void*> frameAddresses(maxFrames);
            const int capturedFrameCount = ::backtrace(frameAddresses.data(), static_cast<int>(frameAddresses.size()));
            if (capturedFrameCount <= 0)
                return {};

            char** symbols = ::backtrace_symbols(frameAddresses.data(), capturedFrameCount);
            std::vector<std::string> frames;
            frames.reserve(static_cast<std::size_t>(capturedFrameCount));

            for (int frameIndex = 0; frameIndex < capturedFrameCount; ++frameIndex)
            {
                if (symbols != nullptr && symbols[frameIndex] != nullptr)
                {
                    frames.emplace_back(symbols[frameIndex]);
                }
                else
                {
                    frames.emplace_back("[" + std::to_string(frameIndex) + "] " + ToHexString(reinterpret_cast<std::uintptr_t>(frameAddresses[frameIndex])));
                }
            }

            auto* symbolsBuffer = reinterpret_cast<unsigned char*>(symbols);
            std::free(symbolsBuffer);
            return frames;
        }
#else
        std::vector<std::string> CaptureStackTrace(std::size_t)
        {
            return {};
        }
#endif

        std::filesystem::path ResolveReportDirectory(const CrashReportingSpecification& specification)
        {
            std::filesystem::path reportDirectory = specification.ReportDirectory.empty()
                ? std::filesystem::path("crashes")
                : std::filesystem::path(specification.ReportDirectory);

            if (reportDirectory.is_relative())
                return std::filesystem::absolute(reportDirectory);

            return reportDirectory;
        }

#if defined(LIFE_PLATFORM_WINDOWS)
        std::filesystem::path WriteWindowsMiniDump(const std::filesystem::path& reportBasePath, EXCEPTION_POINTERS* exceptionPointers, const CrashReportingSpecification& specification)
        {
            if (exceptionPointers == nullptr || !specification.WriteMiniDump)
                return {};

            std::filesystem::path minidumpPath = reportBasePath;
            minidumpPath += ".dmp";

            const HANDLE fileHandle = ::CreateFileW(
                minidumpPath.wstring().c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (fileHandle == INVALID_HANDLE_VALUE)
                return {};

            MINIDUMP_EXCEPTION_INFORMATION exceptionInformation{};
            exceptionInformation.ThreadId = ::GetCurrentThreadId();
            exceptionInformation.ExceptionPointers = exceptionPointers;
            exceptionInformation.ClientPointers = FALSE;

            const BOOL writeResult = ::MiniDumpWriteDump(
                ::GetCurrentProcess(),
                ::GetCurrentProcessId(),
                fileHandle,
                static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
                &exceptionInformation,
                nullptr,
                nullptr);

            ::CloseHandle(fileHandle);

            if (writeResult == FALSE)
            {
                std::error_code errorCode;
                std::filesystem::remove(minidumpPath, errorCode);
                return {};
            }

            return minidumpPath;
        }
#endif

        std::filesystem::path WriteCrashReport(const CrashEvent& event
#if defined(LIFE_PLATFORM_WINDOWS)
            , EXCEPTION_POINTERS* exceptionPointers
#endif
        )
        {
            const std::shared_ptr<CrashConfigurationSnapshot> snapshot = LoadSnapshot();
            const CrashReportingSpecification& specification = snapshot->Specification;
            if (!specification.Enabled || !specification.WriteReport)
                return {};

            try
            {
                FlushLogs();
                EnsurePlatformMetadataAvailable();

                const std::filesystem::path reportDirectory = ResolveReportDirectory(specification);
                std::filesystem::create_directories(reportDirectory);

                const std::string fileStem = SanitizeFileComponent(snapshot->ApplicationName) +
                    "_" + GetCurrentTimestampToken() +
                    "_pid" + std::to_string(PlatformUtils::GetCurrentProcessId()) +
                    "_" + SanitizeFileComponent(event.Category);

                std::filesystem::path reportBasePath = reportDirectory / fileStem;
                std::filesystem::path reportPath = reportBasePath;
                reportPath += ".crash.txt";

#if defined(LIFE_PLATFORM_WINDOWS)
                const std::filesystem::path minidumpPath = WriteWindowsMiniDump(reportBasePath, exceptionPointers, specification);
#endif

                std::ofstream reportStream(reportPath, std::ios::out | std::ios::trunc);
                if (!reportStream.is_open())
                    return {};

                reportStream << "Life Crash Report\n";
                reportStream << "Timestamp: " << GetCurrentTimestampDisplayString() << '\n';
                reportStream << "Application: " << snapshot->ApplicationName << '\n';
                reportStream << "Category: " << event.Category << '\n';
                reportStream << "Phase: " << event.Phase << '\n';
                reportStream << "Reason: " << event.Reason << '\n';
                reportStream << "ProcessId: " << PlatformUtils::GetCurrentProcessId() << '\n';
                reportStream << "ThreadId: " << GetThreadIdString() << '\n';
                reportStream << "WorkingDirectory: " << std::filesystem::current_path().string() << '\n';
                reportStream << "CommandLine: " << JoinCommandLine(snapshot->CommandLine) << '\n';

                if (event.SignalNumber != 0)
                    reportStream << "Signal: " << SignalName(event.SignalNumber) << " (" << event.SignalNumber << ")\n";

                if (event.FaultAddress != 0)
                    reportStream << "FaultAddress: " << ToHexString(event.FaultAddress) << '\n';

#if defined(LIFE_PLATFORM_WINDOWS)
                if (event.WindowsExceptionCode != 0)
                {
                    reportStream << "WindowsExceptionCode: " << ToHexString(event.WindowsExceptionCode)
                                 << " (" << WindowsExceptionCodeToString(event.WindowsExceptionCode) << ")\n";
                }

                if (!minidumpPath.empty())
                    reportStream << "Minidump: " << minidumpPath.string() << '\n';
#endif

                if (PlatformDetection::IsInitialized())
                {
                    const PlatformInfo& platformInfo = PlatformDetection::GetPlatformInfo();
                    reportStream << "Platform: " << platformInfo.platformName << '\n';
                    reportStream << "Architecture: " << platformInfo.architectureName << '\n';
                    reportStream << "Compiler: " << platformInfo.compilerName << " " << platformInfo.compilerVersion << '\n';
                    reportStream << "OS: " << platformInfo.osName << " " << platformInfo.osVersion << '\n';
                    reportStream << "ExecutablePath: " << platformInfo.executablePath << '\n';
                    reportStream << "DetectedWorkingDirectory: " << platformInfo.workingDirectory << '\n';
                    reportStream << "BuildType: " << platformInfo.buildType << '\n';
                    reportStream << "BuildDate: " << platformInfo.buildDate << ' ' << platformInfo.buildTime << '\n';
                }

                const LogSpecification logSpecification = Log::GetSpecification();
                reportStream << "FileLoggingEnabled: " << (logSpecification.EnableFile ? "true" : "false") << '\n';
                if (logSpecification.EnableFile)
                    reportStream << "LogFile: " << logSpecification.FilePath << '\n';

                reportStream << '\n';
                reportStream << "Details:\n";
                reportStream << event.Details << '\n';
                reportStream << '\n';
                reportStream << "StackTrace:\n";
                if (event.StackTrace.empty())
                {
                    reportStream << "<unavailable>\n";
                }
                else
                {
                    for (const std::string& frame : event.StackTrace)
                        reportStream << frame << '\n';
                }

                reportStream.flush();
                reportStream.close();

                StoreLastReportPath(reportPath);

                return reportPath;
            }
            catch (...)
            {
                return {};
            }
        }

        void HandleTerminate()
        {
            CrashDiagnosticsState& state = GetState();
            if (state.HandlingCrash.exchange(true))
                std::_Exit(EXIT_FAILURE);

            CrashEvent event;
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
                    event.Details = BuildExceptionDetails(exception);
                }
                catch (...)
                {
                    event.Reason = "Unknown non-standard exception";
                    event.Details = "std::terminate was invoked with a non-standard active exception.";
                }
            }

            event.StackTrace = CaptureStackTrace(LoadSnapshot()->Specification.MaxStackFrames);
            WriteCrashReport(
                event
#if defined(LIFE_PLATFORM_WINDOWS)
                , nullptr
#endif
            );
            FlushLogs();
            std::_Exit(EXIT_FAILURE);
        }

#if defined(LIFE_PLATFORM_WINDOWS)
        LONG WINAPI HandleUnhandledStructuredException(EXCEPTION_POINTERS* exceptionPointers)
        {
            CrashDiagnosticsState& state = GetState();
            if (state.HandlingCrash.exchange(true))
                return EXCEPTION_EXECUTE_HANDLER;

            CrashEvent event;
            event.Category = "structured-exception";
            event.Phase = "SetUnhandledExceptionFilter";
            event.WindowsExceptionCode = exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr
                ? exceptionPointers->ExceptionRecord->ExceptionCode
                : 0;
            event.Reason = event.WindowsExceptionCode != 0
                ? WindowsExceptionCodeToString(event.WindowsExceptionCode)
                : "Unknown Windows exception";
            event.Details = "Unhandled structured exception captured by the engine crash filter.";
            if (exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr)
            {
                event.FaultAddress = reinterpret_cast<std::uintptr_t>(exceptionPointers->ExceptionRecord->ExceptionAddress);
            }

            event.StackTrace = CaptureStackTrace(LoadSnapshot()->Specification.MaxStackFrames);
            WriteCrashReport(event, exceptionPointers);
            FlushLogs();
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

            CrashEvent event;
            event.Category = "signal";
            event.Phase = "sigaction";
            event.SignalNumber = signalNumber;
            event.Reason = SignalName(signalNumber);
            event.Details = "Fatal signal captured by the engine crash handler.";
            if (signalInfo != nullptr)
                event.FaultAddress = reinterpret_cast<std::uintptr_t>(signalInfo->si_addr);

            event.StackTrace = CaptureStackTrace(LoadSnapshot()->Specification.MaxStackFrames);
            WriteCrashReport(event);
            FlushLogs();

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

    void CrashDiagnostics::Install()
    {
        CrashDiagnosticsState& state = GetState();
        std::scoped_lock lock(state.Mutex);
        if (state.Installed)
            return;

        InstallHandlersLocked(state, LoadSnapshot()->Specification);
    }

    void CrashDiagnostics::Shutdown()
    {
        CrashDiagnosticsState& state = GetState();
        std::scoped_lock lock(state.Mutex);
        ShutdownHandlersLocked(state);
    }

    void CrashDiagnostics::Configure(const CrashReportingSpecification& specification)
    {
        CrashDiagnosticsState& state = GetState();
        std::scoped_lock lock(state.Mutex);

        const std::shared_ptr<CrashConfigurationSnapshot> currentSnapshot = LoadSnapshot();
        std::shared_ptr<CrashConfigurationSnapshot> updatedSnapshot = std::make_shared<CrashConfigurationSnapshot>(*currentSnapshot);
        updatedSnapshot->Specification = specification;
        StoreSnapshot(std::move(updatedSnapshot));

        if (state.Installed)
        {
            ShutdownHandlersLocked(state);
            InstallHandlersLocked(state, specification);
        }
    }

    CrashReportingSpecification CrashDiagnostics::GetSpecification()
    {
        return LoadSnapshot()->Specification;
    }

    bool CrashDiagnostics::IsInstalled()
    {
        std::scoped_lock lock(GetState().Mutex);
        return GetState().Installed;
    }

    void CrashDiagnostics::SetApplicationInfo(std::string applicationName, std::vector<std::string> commandLine)
    {
        CrashDiagnosticsState& state = GetState();
        std::scoped_lock lock(state.Mutex);

        const std::shared_ptr<CrashConfigurationSnapshot> currentSnapshot = LoadSnapshot();
        std::shared_ptr<CrashConfigurationSnapshot> updatedSnapshot = std::make_shared<CrashConfigurationSnapshot>(*currentSnapshot);
        updatedSnapshot->ApplicationName = applicationName.empty() ? "Life Application" : std::move(applicationName);
        updatedSnapshot->CommandLine = std::move(commandLine);
        StoreSnapshot(std::move(updatedSnapshot));
    }

    std::filesystem::path CrashDiagnostics::ReportHandledException(const std::exception& exception, std::string_view phase)
    {
        CrashEvent event;
        event.Category = "handled-exception";
        event.Phase = std::string(phase.empty() ? std::string_view("exception") : phase);
        event.Reason = exception.what();
        event.Details = BuildExceptionDetails(exception);
        event.StackTrace = CaptureStackTrace(LoadSnapshot()->Specification.MaxStackFrames);
        return WriteCrashReport(
            event
#if defined(LIFE_PLATFORM_WINDOWS)
            , nullptr
#endif
        );
    }

    std::filesystem::path CrashDiagnostics::ReportMessage(CrashMessageReportSpecification specification)
    {
        CrashEvent event;
        event.Category = std::string(specification.Category.empty() ? std::string_view("report") : specification.Category);
        event.Phase = std::string(specification.Phase.empty() ? std::string_view("message") : specification.Phase);
        event.Reason = std::string(specification.Message);
        event.Details = std::string(specification.Message);
        event.StackTrace = CaptureStackTrace(LoadSnapshot()->Specification.MaxStackFrames);
        return WriteCrashReport(
            event
#if defined(LIFE_PLATFORM_WINDOWS)
            , nullptr
#endif
        );
    }

    std::filesystem::path CrashDiagnostics::GetLastReportPath()
    {
        std::scoped_lock lock(GetState().Mutex);
        return GetState().LastReportPath;
    }
}
