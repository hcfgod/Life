#include "Core/Log.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Life
{
    namespace
    {
        std::pair<std::shared_ptr<spdlog::logger>, std::shared_ptr<spdlog::logger>> CreateLoggers(const LogSpecification& specification)
        {
            std::vector<spdlog::sink_ptr> sinks;
            if (specification.EnableConsole)
            {
                sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
            }

            if (specification.EnableFile)
            {
                if (specification.FilePath.empty())
                    throw std::runtime_error("Log file output is enabled but no file path was provided.");

                const std::filesystem::path logFilePath = specification.FilePath;
                if (logFilePath.has_parent_path())
                    std::filesystem::create_directories(logFilePath.parent_path());

                sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    specification.FilePath,
                    specification.MaxFileSize,
                    static_cast<std::size_t>(specification.MaxFileCount)));
            }

            if (sinks.empty())
                sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

            for (const auto& sink : sinks)
                sink->set_pattern(specification.Pattern);

            std::shared_ptr<spdlog::logger> coreLogger = std::make_shared<spdlog::logger>(specification.CoreLoggerName, sinks.begin(), sinks.end());
            coreLogger->set_level(specification.CoreLevel);
            coreLogger->flush_on(specification.FlushLevel);

            std::shared_ptr<spdlog::logger> clientLogger = std::make_shared<spdlog::logger>(specification.ClientLoggerName, sinks.begin(), sinks.end());
            clientLogger->set_level(specification.ClientLevel);
            clientLogger->flush_on(specification.FlushLevel);

            return { std::move(coreLogger), std::move(clientLogger) };
        }
    }

    std::mutex Log::s_Mutex;
    bool Log::s_Initialized = false;
    std::atomic<std::shared_ptr<spdlog::logger>> Log::s_CoreLogger;
    std::atomic<std::shared_ptr<spdlog::logger>> Log::s_ClientLogger;

    LogSpecification& Log::GetMutableSpecification()
    {
        static std::optional<LogSpecification> specification;
        if (!specification.has_value())
        {
            specification.emplace();
        }

        return *specification;
    }

    void Log::Init()
    {
        std::scoped_lock lock(s_Mutex);
        EnsureInitializedLocked();
    }

    void Log::Configure(const LogSpecification& specification)
    {
        std::scoped_lock lock(s_Mutex);
        ReinitializeLocked(specification);
    }

    LogSpecification Log::GetSpecification()
    {
        std::scoped_lock lock(s_Mutex);
        return GetMutableSpecification();
    }

    std::shared_ptr<spdlog::logger> Log::GetCoreLogger()
    {
        std::shared_ptr<spdlog::logger> logger = s_CoreLogger.load(std::memory_order_acquire);
        if (logger != nullptr)
            return logger;

        std::scoped_lock lock(s_Mutex);
        EnsureInitializedLocked();
        return s_CoreLogger.load(std::memory_order_acquire);
    }

    std::shared_ptr<spdlog::logger> Log::GetClientLogger()
    {
        std::shared_ptr<spdlog::logger> logger = s_ClientLogger.load(std::memory_order_acquire);
        if (logger != nullptr)
            return logger;

        std::scoped_lock lock(s_Mutex);
        EnsureInitializedLocked();
        return s_ClientLogger.load(std::memory_order_acquire);
    }

    void Log::EnsureInitializedLocked()
    {
        if (s_Initialized && s_CoreLogger.load(std::memory_order_acquire) && s_ClientLogger.load(std::memory_order_acquire))
            return;

        ReinitializeLocked(GetMutableSpecification());
    }

    void Log::ReinitializeLocked(const LogSpecification& specification)
    {
        auto [coreLogger, clientLogger] = CreateLoggers(specification);

        s_CoreLogger.store(std::move(coreLogger), std::memory_order_release);
        s_ClientLogger.store(std::move(clientLogger), std::memory_order_release);
        GetMutableSpecification() = specification;
        s_Initialized = true;
    }
}
