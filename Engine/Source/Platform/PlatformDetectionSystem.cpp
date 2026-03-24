#include "Platform/PlatformDetection.h"

#ifdef LIFE_PLATFORM_WINDOWS
    #include <Windows.h>
    #include <winternl.h>
#elif defined(LIFE_PLATFORM_MACOS)
    #include <sys/sysctl.h>
    #include <sys/utsname.h>
#elif defined(LIFE_PLATFORM_LINUX)
    #include <sys/utsname.h>
#endif

namespace Life
{
    void PlatformDetection::DetectPlatform()
    {
        #ifdef LIFE_PLATFORM_WINDOWS
            s_PlatformInfo.platform = Platform::Windows;
            s_PlatformInfo.platformName = "Windows";
        #elif defined(LIFE_PLATFORM_MACOS)
            s_PlatformInfo.platform = Platform::macOS;
            s_PlatformInfo.platformName = "macOS";
        #elif defined(LIFE_PLATFORM_LINUX)
            s_PlatformInfo.platform = Platform::Linux;
            s_PlatformInfo.platformName = "Linux";
        #else
            s_PlatformInfo.platform = Platform::Unknown;
            s_PlatformInfo.platformName = "Unknown";
        #endif
    }

    void PlatformDetection::DetectArchitecture()
    {
        #ifdef LIFE_ARCHITECTURE_X64
            s_PlatformInfo.architecture = Architecture::x64;
            s_PlatformInfo.architectureName = "x64";
        #elif defined(LIFE_ARCHITECTURE_X86)
            s_PlatformInfo.architecture = Architecture::x86;
            s_PlatformInfo.architectureName = "x86";
        #elif defined(LIFE_ARCHITECTURE_ARM64)
            s_PlatformInfo.architecture = Architecture::ARM64;
            s_PlatformInfo.architectureName = "ARM64";
        #elif defined(LIFE_ARCHITECTURE_ARM32)
            s_PlatformInfo.architecture = Architecture::ARM32;
            s_PlatformInfo.architectureName = "ARM32";
        #else
            s_PlatformInfo.architecture = Architecture::Unknown;
            s_PlatformInfo.architectureName = "Unknown";
        #endif
    }

    void PlatformDetection::DetectCompiler()
    {
        #ifdef LIFE_COMPILER_MSVC
            s_PlatformInfo.compiler = Compiler::MSVC;
            s_PlatformInfo.compilerName = "MSVC";
            s_PlatformInfo.compilerVersion = std::to_string(_MSC_VER);
        #elif defined(LIFE_COMPILER_GCC)
            s_PlatformInfo.compiler = Compiler::GCC;
            s_PlatformInfo.compilerName = "GCC";
            s_PlatformInfo.compilerVersion = std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
        #elif defined(LIFE_COMPILER_APPLE_CLANG)
            s_PlatformInfo.compiler = Compiler::AppleClang;
            s_PlatformInfo.compilerName = "AppleClang";
            s_PlatformInfo.compilerVersion = std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
        #elif defined(LIFE_COMPILER_CLANG)
            s_PlatformInfo.compiler = Compiler::Clang;
            s_PlatformInfo.compilerName = "Clang";
            s_PlatformInfo.compilerVersion = std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
        #else
            s_PlatformInfo.compiler = Compiler::Unknown;
            s_PlatformInfo.compilerName = "Unknown";
            s_PlatformInfo.compilerVersion = "Unknown";
        #endif
    }

    void PlatformDetection::DetectOS()
    {
        #ifdef LIFE_PLATFORM_WINDOWS
            s_PlatformInfo.osName = "Windows";
            s_PlatformInfo.osVersion = "Unknown";
            s_PlatformInfo.osBuild = "Unknown";

            RTL_OSVERSIONINFOW osvi{};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
            const HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");
            if (ntdllModule != nullptr)
            {
                const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdllModule, "RtlGetVersion"));
                if (rtlGetVersion != nullptr && rtlGetVersion(&osvi) >= 0)
                {
                    s_PlatformInfo.osVersion = std::to_string(osvi.dwMajorVersion) + "." + std::to_string(osvi.dwMinorVersion);
                    s_PlatformInfo.osBuild = std::to_string(osvi.dwBuildNumber);
                }
            }
        #elif defined(LIFE_PLATFORM_MACOS)
            s_PlatformInfo.osName = "macOS";

            char version[256];
            size_t size = sizeof(version);
            if (sysctlbyname("kern.osrelease", version, &size, nullptr, 0) == 0)
            {
                s_PlatformInfo.osVersion = version;
            }

            char build[256];
            size = sizeof(build);
            if (sysctlbyname("kern.osversion", build, &size, nullptr, 0) == 0)
            {
                s_PlatformInfo.osBuild = build;
            }
        #elif defined(LIFE_PLATFORM_LINUX)
            struct utsname uts;
            if (uname(&uts) == 0)
            {
                s_PlatformInfo.osName = uts.sysname;
                s_PlatformInfo.osVersion = uts.release;
                s_PlatformInfo.osBuild = uts.version;
            }
            else
            {
                s_PlatformInfo.osName = "Linux";
                s_PlatformInfo.osVersion = "Unknown";
                s_PlatformInfo.osBuild = "Unknown";
            }
        #else
            s_PlatformInfo.osName = "Unknown";
            s_PlatformInfo.osVersion = "Unknown";
            s_PlatformInfo.osBuild = "Unknown";
        #endif
    }

    std::string PlatformDetection::GetPlatformString()
    {
        return s_PlatformInfo.platformName;
    }

    std::string PlatformDetection::GetArchitectureString()
    {
        return s_PlatformInfo.architectureName;
    }

    std::string PlatformDetection::GetCompilerString()
    {
        return s_PlatformInfo.compilerName + " " + s_PlatformInfo.compilerVersion;
    }

    std::string PlatformDetection::GetOSString()
    {
        return s_PlatformInfo.osName + " " + s_PlatformInfo.osVersion;
    }
}
