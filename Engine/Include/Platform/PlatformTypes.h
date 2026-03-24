#pragma once

#include "Platform/PlatformMacros.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Life
{
    enum class Platform
    {
        Unknown = 0,
        Windows,
        macOS,
        Linux,
        Android,
        iOS,
        Web
    };

    enum class Architecture
    {
        Unknown = 0,
        x86,
        x64,
        ARM32,
        ARM64,
        RISC_V
    };

    enum class Compiler
    {
        Unknown = 0,
        MSVC,
        GCC,
        Clang,
        AppleClang
    };

    struct SystemCapabilities
    {
        bool hasSSE2 = false;
        bool hasSSE3 = false;
        bool hasSSE4_1 = false;
        bool hasSSE4_2 = false;
        bool hasAVX = false;
        bool hasAVX2 = false;
        bool hasAVX512 = false;
        bool hasNEON = false;
        bool hasAltiVec = false;

        uint32_t cpuCount = 0;
        uint64_t totalMemory = 0;
        uint64_t availableMemory = 0;

        bool hasOpenGL = false;
        bool hasVulkan = false;
        bool hasMetal = false;
        bool hasDirectX = false;

        std::string gpuVendor;
        std::string gpuRenderer;
        std::string gpuVersion;
    };

    struct PlatformInfo
    {
        Platform platform = Platform::Unknown;
        Architecture architecture = Architecture::Unknown;
        Compiler compiler = Compiler::Unknown;

        std::string platformName;
        std::string architectureName;
        std::string compilerName;
        std::string compilerVersion;

        std::string osName;
        std::string osVersion;
        std::string osBuild;

        SystemCapabilities capabilities;

        std::string executablePath;
        std::string workingDirectory;
        std::string userDataPath;
        std::string tempPath;
        std::string systemPath;

        std::string buildDate;
        std::string buildTime;
        std::string buildType;
        std::string buildVersion;
    };
}
