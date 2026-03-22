#include "Core/Log.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace Life
{
    std::mutex Log::s_Mutex;
    bool Log::s_Initialized = false;
    LogSpecification Log::s_Specification;
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init()
    {
        std::scoped_lock lock(s_Mutex);
        EnsureInitializedLocked();
    }

    void Log::Configure(const LogSpecification& specification)
    {
        std::scoped_lock lock(s_Mutex);
        s_Specification = specification;
        ReinitializeLocked();
    }

    LogSpecification Log::GetSpecification()
    {
        std::scoped_lock lock(s_Mutex);
        return s_Specification;
    }

    std::shared_ptr<spdlog::logger> Log::GetCoreLogger()
    {
        std::scoped_lock lock(s_Mutex);
        EnsureInitializedLocked();
        return s_CoreLogger;
    }

    std::shared_ptr<spdlog::logger> Log::GetClientLogger()
    {
        std::scoped_lock lock(s_Mutex);
        EnsureInitializedLocked();
        return s_ClientLogger;
    }

    void Log::EnsureInitializedLocked()
    {
        if (s_Initialized && s_CoreLogger && s_ClientLogger)
            return;

        ReinitializeLocked();
    }

    void Log::ReinitializeLocked()
    {
        auto unregisterLogger = [](std::shared_ptr<spdlog::logger>& logger)
        {
            if (!logger)
                return;

            spdlog::drop(logger->name());
            logger.reset();
        };

        unregisterLogger(s_CoreLogger);
        unregisterLogger(s_ClientLogger);

        std::vector<spdlog::sink_ptr> sinks;
        if (s_Specification.EnableConsole)
        {
            sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }

        if (s_Specification.EnableFile)
        {
            if (s_Specification.FilePath.empty())
                throw std::runtime_error("Log file output is enabled but no file path was provided.");

            const std::filesystem::path logFilePath = s_Specification.FilePath;
            if (logFilePath.has_parent_path())
                std::filesystem::create_directories(logFilePath.parent_path());

            sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                s_Specification.FilePath,
                s_Specification.MaxFileSize,
                static_cast<std::size_t>(s_Specification.MaxFileCount)));
        }

        if (sinks.empty())
            sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

        for (const auto& sink : sinks)
            sink->set_pattern(s_Specification.Pattern);

        s_CoreLogger = std::make_shared<spdlog::logger>(s_Specification.CoreLoggerName, sinks.begin(), sinks.end());
        s_CoreLogger->set_level(s_Specification.CoreLevel);
        s_CoreLogger->flush_on(s_Specification.FlushLevel);
        spdlog::register_logger(s_CoreLogger);

        s_ClientLogger = std::make_shared<spdlog::logger>(s_Specification.ClientLoggerName, sinks.begin(), sinks.end());
        s_ClientLogger->set_level(s_Specification.ClientLevel);
        s_ClientLogger->flush_on(s_Specification.FlushLevel);
        spdlog::register_logger(s_ClientLogger);

        s_Initialized = true;
    }
}
