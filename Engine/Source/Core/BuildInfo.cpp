#include "Core/BuildInfo.h"

#include "Platform/PlatformDetection.h"

#include <sstream>

#ifndef LIFE_BUILD_VERSION
    #define LIFE_BUILD_VERSION "local"
#endif

#ifndef LIFE_BUILD_COMMIT
    #define LIFE_BUILD_COMMIT "unknown"
#endif

#ifndef LIFE_BUILD_DATE
    #define LIFE_BUILD_DATE __DATE__ " " __TIME__
#endif

#ifndef LIFE_BUILD_ARCHITECTURE
    #define LIFE_BUILD_ARCHITECTURE "unknown"
#endif

#ifndef LIFE_BUILD_CONFIGURATION
    #define LIFE_BUILD_CONFIGURATION "unknown"
#endif

namespace Life
{
    namespace
    {
        std::string ResolveBuildPlatform()
        {
#if defined(LIFE_PLATFORM_WINDOWS)
            return "Windows";
#elif defined(LIFE_PLATFORM_MACOS)
            return "macOS";
#elif defined(LIFE_PLATFORM_LINUX)
            return "Linux";
#else
            return "unknown";
#endif
        }

        std::string ResolveBuildCompiler()
        {
#if defined(LIFE_COMPILER_MSVC)
            return "MSVC " + std::to_string(_MSC_VER);
#elif defined(LIFE_COMPILER_APPLE_CLANG)
            return "AppleClang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(LIFE_COMPILER_CLANG)
            return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(LIFE_COMPILER_GCC)
            return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
            return "unknown";
#endif
        }
    }

    const BuildInfo& GetBuildInfo()
    {
        static const BuildInfo info = []()
        {
            BuildInfo buildInfo;
            buildInfo.Version = LIFE_BUILD_VERSION;
            buildInfo.Commit = LIFE_BUILD_COMMIT;
            buildInfo.BuildDate = LIFE_BUILD_DATE;
            buildInfo.Platform = ResolveBuildPlatform();
            buildInfo.Architecture = LIFE_BUILD_ARCHITECTURE;
            buildInfo.Configuration = LIFE_BUILD_CONFIGURATION;
            buildInfo.Compiler = ResolveBuildCompiler();
            return buildInfo;
        }();

        return info;
    }

    std::string FormatBuildVersionLine()
    {
        const BuildInfo& info = GetBuildInfo();
        return "Life " + info.Version + " (" + info.Commit + ", " + info.Configuration + ", " + info.Platform + " " + info.Architecture + ")";
    }

    std::string FormatBuildDiagnostics()
    {
        if (!PlatformDetection::IsInitialized())
            PlatformDetection::Initialize();

        const BuildInfo& info = GetBuildInfo();
        const PlatformInfo& platform = PlatformDetection::GetPlatformInfo();

        std::ostringstream stream;
        stream << "Version: " << info.Version << '\n';
        stream << "Commit: " << info.Commit << '\n';
        stream << "BuildDate: " << info.BuildDate << '\n';
        stream << "Configuration: " << info.Configuration << '\n';
        stream << "Platform: " << info.Platform << '\n';
        stream << "Architecture: " << info.Architecture << '\n';
        stream << "Compiler: " << info.Compiler << '\n';
        stream << "OS: " << platform.osName << ' ' << platform.osVersion << '\n';
        stream << "ExecutablePath: " << platform.executablePath << '\n';
        stream << "WorkingDirectory: " << platform.workingDirectory << '\n';
        stream << "CPUCount: " << platform.capabilities.cpuCount << '\n';
        stream << "TotalMemory: " << platform.capabilities.totalMemory << '\n';
        return stream.str();
    }
}
