#pragma once

#include "Platform/PlatformTypes.h"

#include <cstdint>
#include <string>

namespace Life
{
    class PlatformDetection
    {
    public:
        static const PlatformInfo& GetPlatformInfo();

        static bool IsWindows() { return GetPlatformInfo().platform == Platform::Windows; }
        static bool IsMacOS() { return GetPlatformInfo().platform == Platform::macOS; }
        static bool IsLinux() { return GetPlatformInfo().platform == Platform::Linux; }
        static bool IsAndroid() { return GetPlatformInfo().platform == Platform::Android; }
        static bool IsIOS() { return GetPlatformInfo().platform == Platform::iOS; }
        static bool IsWeb() { return GetPlatformInfo().platform == Platform::Web; }

        static bool IsX86() { return GetPlatformInfo().architecture == Architecture::x86; }
        static bool IsX64() { return GetPlatformInfo().architecture == Architecture::x64; }
        static bool IsARM32() { return GetPlatformInfo().architecture == Architecture::ARM32; }
        static bool IsARM64() { return GetPlatformInfo().architecture == Architecture::ARM64; }
        static bool IsRISC_V() { return GetPlatformInfo().architecture == Architecture::RISC_V; }

        static bool IsMSVC() { return GetPlatformInfo().compiler == Compiler::MSVC; }
        static bool IsGCC() { return GetPlatformInfo().compiler == Compiler::GCC; }
        static bool IsClang() { return GetPlatformInfo().compiler == Compiler::Clang; }
        static bool IsAppleClang() { return GetPlatformInfo().compiler == Compiler::AppleClang; }

        static bool HasSSE2() { return GetPlatformInfo().capabilities.hasSSE2; }
        static bool HasSSE3() { return GetPlatformInfo().capabilities.hasSSE3; }
        static bool HasSSE4_1() { return GetPlatformInfo().capabilities.hasSSE4_1; }
        static bool HasSSE4_2() { return GetPlatformInfo().capabilities.hasSSE4_2; }
        static bool HasAVX() { return GetPlatformInfo().capabilities.hasAVX; }
        static bool HasAVX2() { return GetPlatformInfo().capabilities.hasAVX2; }
        static bool HasAVX512() { return GetPlatformInfo().capabilities.hasAVX512; }
        static bool HasNEON() { return GetPlatformInfo().capabilities.hasNEON; }
        static bool HasAltiVec() { return GetPlatformInfo().capabilities.hasAltiVec; }

        static bool HasOpenGL() { return GetPlatformInfo().capabilities.hasOpenGL; }
        static bool HasVulkan() { return GetPlatformInfo().capabilities.hasVulkan; }
        static bool HasMetal() { return GetPlatformInfo().capabilities.hasMetal; }
        static bool HasDirectX() { return GetPlatformInfo().capabilities.hasDirectX; }

        static uint32_t GetCPUCount() { return GetPlatformInfo().capabilities.cpuCount; }
        static uint64_t GetTotalMemory() { return GetPlatformInfo().capabilities.totalMemory; }
        static uint64_t GetAvailableMemory() { return GetPlatformInfo().capabilities.availableMemory; }

        static std::string GetExecutablePath() { return GetPlatformInfo().executablePath; }
        static std::string GetWorkingDirectory() { return GetPlatformInfo().workingDirectory; }
        static std::string GetUserDataPath() { return GetPlatformInfo().userDataPath; }
        static std::string GetTempPath() { return GetPlatformInfo().tempPath; }
        static std::string GetSystemPath() { return GetPlatformInfo().systemPath; }

        static std::string GetPlatformString();
        static std::string GetArchitectureString();
        static std::string GetCompilerString();
        static std::string GetOSString();

        static void Initialize();
        static void RefreshCapabilities();
        static bool IsInitialized();

    private:
        static PlatformInfo s_PlatformInfo;
        static bool s_Initialized;

        static void DetectPlatform();
        static void DetectArchitecture();
        static void DetectCompiler();
        static void DetectOS();
        static void DetectCapabilities();
        static void DetectPaths();
        static void DetectGraphicsAPIs();
    };
}
