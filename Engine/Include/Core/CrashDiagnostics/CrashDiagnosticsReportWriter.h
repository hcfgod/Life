#pragma once

#include "Core/CrashDiagnostics/CrashDiagnosticsState.h"

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace Life::CrashDiagnosticsDetail
{
    std::string BuildCrashExceptionDetails(const std::exception& exception);
    std::vector<std::string> CaptureCrashStackTrace(std::size_t maxFrames);
    void FlushCrashDiagnosticLoggers();
    std::string DescribeCrashSignal(int signalNumber);
#if defined(LIFE_PLATFORM_WINDOWS)
    std::string DescribeWindowsExceptionCode(std::uint32_t exceptionCode);
    std::filesystem::path WriteCrashDiagnosticsReport(const CrashDiagnosticsEvent& event);
    std::filesystem::path WriteCrashDiagnosticsReport(
        const CrashDiagnosticsEvent& event
        , EXCEPTION_POINTERS* exceptionPointers
    );
#else
    std::filesystem::path WriteCrashDiagnosticsReport(const CrashDiagnosticsEvent& event);
#endif
}
