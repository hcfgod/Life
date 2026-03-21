#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace Life
{
    class Log
    {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
        static std::shared_ptr<spdlog::logger>& GetClientLogger();

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}

#define LOG_CORE_TRACE(...) ::Life::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_CORE_INFO(...) ::Life::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_CORE_WARN(...) ::Life::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_CORE_ERROR(...) ::Life::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_TRACE(...) ::Life::Log::GetClientLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...) ::Life::Log::GetClientLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) ::Life::Log::GetClientLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::Life::Log::GetClientLogger()->error(__VA_ARGS__)
