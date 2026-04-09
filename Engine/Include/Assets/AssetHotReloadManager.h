#pragma once

#include "Assets/AssetDatabase.h"
#include "Assets/AssetTreeWatcher.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetHotReloadManager
    // Watches asset files for changes, triggers re-imports, reloads cached assets,
    // and propagates dependency cascades with debounced reload.
    // -----------------------------------------------------------------------------
    class AssetHotReloadManager final
    {
    public:
        static AssetHotReloadManager& GetInstance()
        {
            static AssetHotReloadManager s_Instance;
            return s_Instance;
        }

        void Enable(bool enable);
        bool IsEnabled() const { return m_Enabled.load(std::memory_order_relaxed); }

        void WatchKey(const std::string& key);
        void RequestReload(const std::string& key, const std::string& guid = {});
        void Pump();
        void Shutdown();

        void SetDebounceWindow(std::chrono::milliseconds window) { m_DebounceWindow = window; }

    private:
        AssetHotReloadManager() = default;
        ~AssetHotReloadManager() { Shutdown(); }

        AssetHotReloadManager(const AssetHotReloadManager&) = delete;
        AssetHotReloadManager& operator=(const AssetHotReloadManager&) = delete;

        void OnFileChanged(const std::filesystem::path& changedPath);
        void EnqueueReloadLocked(const std::string& key, const std::string& guid);
        void ReloadThreadMain();
        void EnsureWatcherRunningLocked();

        struct WatchEntry
        {
            std::string key;
            std::string guid;
            std::string resolvedPath;
            AssetType type = AssetType::Unknown;
            nlohmann::json importerSettings = nlohmann::json::object();
        };

        struct PendingReload
        {
            std::string key;
            std::string guid;
            uint32_t generation = 0;
            std::chrono::steady_clock::time_point dueTime;
        };

        std::atomic<bool> m_Enabled{false};
        std::chrono::milliseconds m_DebounceWindow{300};

        std::mutex m_Mutex;
        std::unordered_map<std::string, WatchEntry> m_ByResolvedPath;
        std::unordered_map<std::string, PendingReload> m_PendingByKey;
        std::deque<PendingReload> m_ReadyReloads;

        std::unique_ptr<AssetTreeWatcher> m_TreeWatcher;

        bool m_ReloadThreadRunning = false;
        bool m_ReloadThreadStop = false;
        std::thread m_ReloadThread;
        std::condition_variable m_ReloadCv;
    };
}
