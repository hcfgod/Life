#include "Assets/AssetTreeWatcher.h"

#include "Core/Log.h"

#include <chrono>
#include <unordered_map>

namespace Life::Assets
{
    void AssetTreeWatcher::Start(const std::filesystem::path& watchRoot, ChangeCallback callback)
    {
        if (m_Running.load(std::memory_order_relaxed))
        {
            Stop();
        }

        if (watchRoot.empty() || !callback)
        {
            return;
        }

        m_WatchRoot = watchRoot;
        m_Callback = std::move(callback);
        m_Running.store(true, std::memory_order_relaxed);
        m_Thread = std::thread(&AssetTreeWatcher::WatchThread, this);
    }

    void AssetTreeWatcher::Stop()
    {
        m_Running.store(false, std::memory_order_relaxed);
        if (m_Thread.joinable())
        {
            m_Thread.join();
        }
    }

    void AssetTreeWatcher::WatchThread()
    {
        // Polling fallback implementation.
        // A production implementation would use ReadDirectoryChangesW (Windows),
        // inotify (Linux), or FSEvents (macOS).
        std::unordered_map<std::string, std::filesystem::file_time_type> lastWriteTimes;

        auto snapshot = [&]()
        {
            std::error_code ec;
            for (auto it = std::filesystem::recursive_directory_iterator(m_WatchRoot, ec);
                 it != std::filesystem::recursive_directory_iterator();
                 it.increment(ec))
            {
                if (ec) { ec.clear(); continue; }
                if (!it->is_regular_file(ec)) { ec.clear(); continue; }

                const auto& path = it->path();
                if (path.extension() == ".meta") continue;

                const auto lastWrite = std::filesystem::last_write_time(path, ec);
                if (ec) { ec.clear(); continue; }

                const std::string key = path.string();
                auto existing = lastWriteTimes.find(key);
                if (existing == lastWriteTimes.end())
                {
                    lastWriteTimes[key] = lastWrite;
                }
                else if (existing->second != lastWrite)
                {
                    existing->second = lastWrite;
                    if (m_Callback)
                    {
                        m_Callback(path);
                    }
                }
            }
        };

        // Initial snapshot (no callbacks).
        {
            std::error_code ec;
            for (auto it = std::filesystem::recursive_directory_iterator(m_WatchRoot, ec);
                 it != std::filesystem::recursive_directory_iterator();
                 it.increment(ec))
            {
                if (ec) { ec.clear(); continue; }
                if (!it->is_regular_file(ec)) { ec.clear(); continue; }

                const auto& path = it->path();
                if (path.extension() == ".meta") continue;

                const auto lastWrite = std::filesystem::last_write_time(path, ec);
                if (ec) { ec.clear(); continue; }

                lastWriteTimes[path.string()] = lastWrite;
            }
        }

        while (m_Running.load(std::memory_order_relaxed))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!m_Running.load(std::memory_order_relaxed)) break;

            try
            {
                snapshot();
            }
            catch (const std::exception& e)
            {
                LOG_CORE_WARN("AssetTreeWatcher: polling error: {}", e.what());
            }
        }
    }
}
