#pragma once

#include "Assets/AssetHotReloadManager.h"
#include "Assets/GeneratedAssetRuntimeRegistry.h"
#include "Core/Error.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace Life::Assets
{
    class AssetContext final
    {
    public:
        AssetContext() = default;
        ~AssetContext() = default;

        AssetContext(const AssetContext&) = delete;
        AssetContext& operator=(const AssetContext&) = delete;

        void SetAssetRootDirectory(const std::filesystem::path& rootDirectory);
        void SetActiveProjectRootDirectory(const std::filesystem::path& rootDirectory);
        void ClearActiveProjectRootDirectory();

        [[nodiscard]] std::optional<std::filesystem::path> TryGetActiveProjectRootDirectory() const;
        [[nodiscard]] Result<std::filesystem::path> FindProjectRootFromWorkingDirectory() const;
        [[nodiscard]] Result<std::filesystem::path> ResolveAssetKeyToPath(const std::string& assetKey) const;

        AssetHotReloadManager& GetHotReloadManager() noexcept { return m_HotReloadManager; }
        const AssetHotReloadManager& GetHotReloadManager() const noexcept { return m_HotReloadManager; }

        GeneratedAssetRuntimeRegistry& GetGeneratedAssetRegistry() noexcept { return m_GeneratedAssetRegistry; }
        const GeneratedAssetRuntimeRegistry& GetGeneratedAssetRegistry() const noexcept { return m_GeneratedAssetRegistry; }

    private:
        static std::filesystem::path NormalizePath(const std::filesystem::path& path);

        mutable std::mutex m_Mutex;
        std::optional<std::filesystem::path> m_AssetRootOverride;
        std::optional<std::filesystem::path> m_ActiveProjectRootOverride;
        AssetHotReloadManager m_HotReloadManager;
        GeneratedAssetRuntimeRegistry m_GeneratedAssetRegistry;
    };

    [[nodiscard]] AssetContext* TryGetActiveAssetContext() noexcept;
    [[nodiscard]] AssetHotReloadManager& GetAssetHotReloadManager() noexcept;
    [[nodiscard]] GeneratedAssetRuntimeRegistry& GetGeneratedAssetRuntimeRegistry() noexcept;
}
