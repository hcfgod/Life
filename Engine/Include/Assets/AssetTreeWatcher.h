#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetTreeWatcher
    // Watches the Assets/ directory tree for file changes and invokes a callback.
    // Uses platform-native directory notifications where available, with a polling
    // fallback for portability.
    // -----------------------------------------------------------------------------
    class AssetTreeWatcher final
    {
    public:
        using ChangeCallback = std::function<void(const std::filesystem::path& changedPath)>;

        AssetTreeWatcher() = default;
        ~AssetTreeWatcher() { Stop(); }

        AssetTreeWatcher(const AssetTreeWatcher&) = delete;
        AssetTreeWatcher& operator=(const AssetTreeWatcher&) = delete;

        void Start(const std::filesystem::path& watchRoot, ChangeCallback callback);
        void Stop();
        bool IsRunning() const { return m_Running.load(std::memory_order_relaxed); }

    private:
        void WatchThread();

        std::filesystem::path m_WatchRoot;
        ChangeCallback m_Callback;
        std::atomic<bool> m_Running{false};
        std::thread m_Thread;
    };
}
