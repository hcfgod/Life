#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

#include <spdlog/spdlog.h>

namespace Life
{
    struct LogSpecification
    {
        std::string CoreLoggerName = "LIFE";
        std::string ClientLoggerName = "APP";
        std::string Pattern = "%^[%Y-%m-%d %T.%e] [thread %t] %n: %v%$";
        spdlog::level::level_enum CoreLevel = spdlog::level::trace;
        spdlog::level::level_enum ClientLevel = spdlog::level::trace;
        spdlog::level::level_enum FlushLevel = spdlog::level::warn;
        bool EnableConsole = true;
        bool EnableFile = true;
        std::string FilePath = "logs/life.log";
        std::size_t MaxFileSize = 5 * 1024 * 1024;
        std::size_t MaxFileCount = 3;
    };

    class Log
    {
    public:
        static void Init();
        static void Configure(const LogSpecification& specification);
        static LogSpecification GetSpecification();

        static std::shared_ptr<spdlog::logger> GetCoreLogger();
        static std::shared_ptr<spdlog::logger> GetClientLogger();

    private:
        static void EnsureInitializedLocked();
        static void ReinitializeLocked(const LogSpecification& specification);
        static LogSpecification& GetMutableSpecification();

        static std::mutex s_Mutex;
        static bool s_Initialized;
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

#define LOG_CORE_TRACE(...) do { if (auto logger = ::Life::Log::GetCoreLogger()) logger->trace(__VA_ARGS__); } while (false)
#define LOG_CORE_INFO(...) do { if (auto logger = ::Life::Log::GetCoreLogger()) logger->info(__VA_ARGS__); } while (false)
#define LOG_CORE_WARN(...) do { if (auto logger = ::Life::Log::GetCoreLogger()) logger->warn(__VA_ARGS__); } while (false)
#define LOG_CORE_ERROR(...) do { if (auto logger = ::Life::Log::GetCoreLogger()) logger->error(__VA_ARGS__); } while (false)
#define LOG_CORE_CRITICAL(...) do { if (auto logger = ::Life::Log::GetCoreLogger()) logger->critical(__VA_ARGS__); } while (false)
#define LOG_TRACE(...) do { if (auto logger = ::Life::Log::GetClientLogger()) logger->trace(__VA_ARGS__); } while (false)
#define LOG_INFO(...) do { if (auto logger = ::Life::Log::GetClientLogger()) logger->info(__VA_ARGS__); } while (false)
#define LOG_WARN(...) do { if (auto logger = ::Life::Log::GetClientLogger()) logger->warn(__VA_ARGS__); } while (false)
#define LOG_ERROR(...) do { if (auto logger = ::Life::Log::GetClientLogger()) logger->error(__VA_ARGS__); } while (false)
#define LOG_CRITICAL(...) do { if (auto logger = ::Life::Log::GetClientLogger()) logger->critical(__VA_ARGS__); } while (false)
