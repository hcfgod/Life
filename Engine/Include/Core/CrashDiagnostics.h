#pragma once

#include <cstddef>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Life
{
    struct CrashReportingSpecification
    {
        bool Enabled = true;
        bool InstallHandlers = true;
        bool CaptureSignals = true;
        bool CaptureTerminate = true;
        bool CaptureUnhandledExceptions = true;
        bool WriteReport = true;
        bool WriteMiniDump = true;
        std::string ReportDirectory = "Crashes";
        std::size_t MaxStackFrames = 64;
    };

    class CrashDiagnostics
    {
    public:
        static void Install();
        static void Shutdown();

        static void Configure(const CrashReportingSpecification& specification);
        static CrashReportingSpecification GetSpecification();

        static bool IsInstalled();
        static void SetApplicationInfo(std::string applicationName, std::vector<std::string> commandLine);

        static std::filesystem::path ReportHandledException(const std::exception& exception, std::string_view phase = {});
        static std::filesystem::path ReportMessage(std::string_view category, std::string_view message, std::string_view phase = {});
        static std::filesystem::path GetLastReportPath();
    };
}
