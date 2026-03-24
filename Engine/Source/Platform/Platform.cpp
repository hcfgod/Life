#include "Platform/PlatformDetection.h"
#include "Core/Log.h"
#include "Core/Error.h"

namespace Life
{
    // Static member initialization
    PlatformInfo PlatformDetection::s_PlatformInfo;
    bool PlatformDetection::s_Initialized = false;

    const PlatformInfo& PlatformDetection::GetPlatformInfo()
    {
        if (!s_Initialized)
        {
            Initialize();
        }
        return s_PlatformInfo;
    }

    void PlatformDetection::Initialize()
    {
        if (s_Initialized)
            return;

        try
        {
            DetectPlatform();
            DetectArchitecture();
            DetectCompiler();
            DetectOS();
            DetectCapabilities();
            DetectPaths();
            DetectGraphicsAPIs();

            s_PlatformInfo.buildDate = __DATE__;
            s_PlatformInfo.buildTime = __TIME__;
            s_PlatformInfo.buildType = LIFE_BUILD_TYPE;

            s_Initialized = true;

            LOG_CORE_INFO("Platform detection initialized:");
            LOG_CORE_INFO("  Platform: {} ({})", s_PlatformInfo.platformName, GetPlatformString());
            LOG_CORE_INFO("  Architecture: {} ({})", s_PlatformInfo.architectureName, GetArchitectureString());
            LOG_CORE_INFO("  Compiler: {} ({})", s_PlatformInfo.compilerName, GetCompilerString());
            LOG_CORE_INFO("  OS: {} ({})", s_PlatformInfo.osName, GetOSString());
            LOG_CORE_INFO("  CPU Cores: {}", s_PlatformInfo.capabilities.cpuCount);
            LOG_CORE_INFO("  Total Memory: {} MB", s_PlatformInfo.capabilities.totalMemory / (static_cast<uint64_t>(1024) * static_cast<uint64_t>(1024)));
        }
        catch (const std::exception& e)
        {
            std::string errorMsg = std::string("Failed to initialize platform detection: ") + e.what();
            PlatformError error(errorMsg, std::source_location::current());
            error.SetFunctionName("PlatformDetection::Initialize");
            error.SetClassName("PlatformDetection");
            error.SetModuleName("Platform");

            LOG_CORE_ERROR("{}", errorMsg);
            Error::LogError(error);
            throw error;
        }
    }

    void PlatformDetection::RefreshCapabilities()
    {
        DetectCapabilities();
        DetectGraphicsAPIs();
    }

    bool PlatformDetection::IsInitialized()
    {
        return s_Initialized;
    }
}