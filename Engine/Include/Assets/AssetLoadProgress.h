#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetLoadProgress
    // Centralized registry for tracking asset loading progress.
    // Thread-safe. Static/global utility (no lifecycle management needed).
    // -----------------------------------------------------------------------------
    class AssetLoadProgress final
    {
    public:
        struct Info
        {
            float Progress = 0.0f;
            std::string Status;
        };

        static void SetProgress(const std::string& key, float progress, const std::string& status = {});
        static void ClearProgress(const std::string& key);
        static std::optional<Info> GetProgress(const std::string& key);
        static std::vector<std::string> GetActiveKeys();

    private:
        static std::mutex s_Mutex;
        static std::unordered_map<std::string, Info> s_Progress;
    };
}
