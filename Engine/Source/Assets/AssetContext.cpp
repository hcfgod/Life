#include "Assets/AssetContext.h"

#include "Assets/AssetPaths.h"
#include "Core/ServiceRegistry.h"

namespace Life::Assets
{
    std::filesystem::path AssetContext::NormalizePath(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        std::error_code ec;
        const std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
        if (!ec)
            return absolutePath.lexically_normal();

        return path.lexically_normal();
    }

    void AssetContext::SetAssetRootDirectory(const std::filesystem::path& rootDirectory)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (rootDirectory.empty())
            m_AssetRootOverride.reset();
        else
            m_AssetRootOverride = NormalizePath(rootDirectory);
    }

    void AssetContext::SetActiveProjectRootDirectory(const std::filesystem::path& rootDirectory)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (rootDirectory.empty())
            m_ActiveProjectRootOverride.reset();
        else
            m_ActiveProjectRootOverride = NormalizePath(rootDirectory);
    }

    void AssetContext::ClearActiveProjectRootDirectory()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_ActiveProjectRootOverride.reset();
    }

    std::optional<std::filesystem::path> AssetContext::TryGetActiveProjectRootDirectory() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_ActiveProjectRootOverride;
    }

    Result<std::filesystem::path> AssetContext::FindProjectRootFromWorkingDirectory() const
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_ActiveProjectRootOverride.has_value())
                return m_ActiveProjectRootOverride.value();
            if (m_AssetRootOverride.has_value())
                return m_AssetRootOverride.value();
        }

        return Assets::FindProjectRootFromWorkingDirectory();
    }

    Result<std::filesystem::path> AssetContext::ResolveAssetKeyToPath(const std::string& assetKey) const
    {
        if (assetKey.empty())
            return Result<std::filesystem::path>(ErrorCode::InvalidArgument, "Asset key is empty");

        std::filesystem::path keyPath(assetKey);
        if (keyPath.is_absolute())
            return NormalizePath(keyPath);

        if (assetKey.rfind("Assets/", 0) == 0 || assetKey.rfind("Assets\\", 0) == 0)
        {
            auto rootResult = FindProjectRootFromWorkingDirectory();
            if (rootResult.IsFailure())
                return rootResult;

            return NormalizePath(rootResult.GetValue() / keyPath);
        }

        std::error_code ec;
        const std::filesystem::path cwd = std::filesystem::current_path(ec);
        if (ec)
            return Result<std::filesystem::path>(ErrorCode::FileAccessDenied, "Failed to query current working directory");

        return NormalizePath(cwd / keyPath);
    }

    AssetContext* TryGetActiveAssetContext() noexcept
    {
        try
        {
            return GetServices().TryGet<AssetContext>();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    AssetHotReloadManager& GetAssetHotReloadManager() noexcept
    {
        if (AssetContext* context = TryGetActiveAssetContext())
            return context->GetHotReloadManager();

        return AssetHotReloadManager::GetInstance();
    }

    GeneratedAssetRuntimeRegistry& GetGeneratedAssetRuntimeRegistry() noexcept
    {
        if (AssetContext* context = TryGetActiveAssetContext())
            return context->GetGeneratedAssetRegistry();

        return GeneratedAssetRuntimeRegistry::GetInstance();
    }
}
