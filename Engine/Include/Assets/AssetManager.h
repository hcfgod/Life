#pragma once

#include "Assets/Asset.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetImporter.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetTypes.h"
#include "Assets/AssetUtils.h"
#include "Assets/AnimationClipAssetImporter.h"
#include "Assets/AnimatorControllerAssetImporter.h"
#include "Assets/AudioClipAssetImporter.h"
#include "Assets/InputActionsAssetImporter.h"
#include "Assets/MaterialAssetImporter.h"
#include "Assets/ShaderAssetImporter.h"
#include "Assets/TextureAssetImporter.h"

#include "Core/Concurrency/AsyncIO.h"
#include "Core/Error.h"
#include "Core/Log.h"
#include "Core/Memory.h"
#include "Core/ServiceRegistry.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetManager
    // Global weak cache for loaded assets, indexed by key and GUID.
    // Supports async loading via importers, retry cooldown, and garbage collection.
    //
    // Host-owned service — create via ApplicationHost, register in ServiceRegistry.
    // -----------------------------------------------------------------------------
    class AssetManager final
    {
    public:
        AssetManager() = default;
        ~AssetManager() = default;

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        void BindDatabase(AssetDatabase& database) noexcept
        {
            std::unique_lock<std::shared_mutex> writeLock(m_Mutex);
            m_Database = &database;
        }

        void UnbindDatabase() noexcept
        {
            std::unique_lock<std::shared_mutex> writeLock(m_Mutex);
            m_Database = nullptr;
        }

        bool HasBoundDatabase() const noexcept
        {
            std::shared_lock<std::shared_mutex> readLock(m_Mutex);
            return m_Database != nullptr;
        }

        // -----------------------------------------------------------------
        // GetOrLoad<T>
        // Returns a cached shared_ptr if still alive, otherwise triggers
        // an async import via AssetImporter<T> and caches the result.
        // -----------------------------------------------------------------
        template<typename T>
        std::shared_ptr<T> GetOrLoad(const std::string& key)
        {
            AssetDatabase* database = nullptr;
            {
                std::shared_lock<std::shared_mutex> readLock(m_Mutex);
                database = m_Database;
            }

            if (database == nullptr)
                return nullptr;

            return GetOrLoad<T>(key, *database);
        }

        template<typename T>
        std::shared_ptr<T> GetOrLoad(const std::string& key, AssetDatabase& db)
        {
            if (key.empty()) return nullptr;

            // Check retry cooldown.
            {
                std::shared_lock<std::shared_mutex> readLock(m_Mutex);
                auto cooldownIt = m_FailedLoadRetryByKey.find(key);
                if (cooldownIt != m_FailedLoadRetryByKey.end())
                {
                    if (std::chrono::steady_clock::now() < cooldownIt->second)
                    {
                        return nullptr;
                    }
                }
            }

            // Try cache.
            {
                std::shared_lock<std::shared_mutex> readLock(m_Mutex);
                if (auto it = m_KeyCache.find(key); it != m_KeyCache.end())
                {
                    if (auto locked = it->second.lock())
                    {
                        return std::static_pointer_cast<T>(locked);
                    }
                }
            }

            // Import.
            auto asset = AssetImporter<T>::Load(key, db);
            if (!asset)
            {
                std::unique_lock<std::shared_mutex> writeLock(m_Mutex);
                m_FailedLoadRetryByKey[key] = std::chrono::steady_clock::now() + m_RetryCooldown;
                return nullptr;
            }

            // Cache.
            {
                std::unique_lock<std::shared_mutex> writeLock(m_Mutex);
                m_KeyCache[key] = asset;
                m_GuidCache[asset->GetGuid()] = asset;
                m_FailedLoadRetryByKey.erase(key);
            }

            return asset;
        }

        // -----------------------------------------------------------------
        // GetByGuid<T>
        // Returns a cached asset by its GUID, if still alive.
        // -----------------------------------------------------------------
        template<typename T>
        std::shared_ptr<T> GetByGuid(const std::string& guid)
        {
            if (guid.empty()) return nullptr;

            std::shared_lock<std::shared_mutex> readLock(m_Mutex);
            auto it = m_GuidCache.find(guid);
            if (it != m_GuidCache.end())
            {
                if (auto locked = it->second.lock())
                {
                    return std::static_pointer_cast<T>(locked);
                }
            }
            return nullptr;
        }

        std::shared_ptr<Asset> GetByGuidAsset(const std::string& guid)
        {
            if (guid.empty()) return nullptr;

            std::shared_lock<std::shared_mutex> readLock(m_Mutex);
            auto it = m_GuidCache.find(guid);
            if (it != m_GuidCache.end())
                return it->second.lock();

            return nullptr;
        }

        // -----------------------------------------------------------------
        // Cache / Register
        // -----------------------------------------------------------------
        void Cache(const std::string& key, const std::string& guid, const std::shared_ptr<Asset>& asset)
        {
            if (!asset) return;
            std::unique_lock<std::shared_mutex> writeLock(m_Mutex);
            if (!key.empty()) m_KeyCache[key] = asset;
            if (!guid.empty()) m_GuidCache[guid] = asset;
        }

        // -----------------------------------------------------------------
        // Garbage collection
        // -----------------------------------------------------------------
        void GarbageCollect()
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);

            for (auto it = m_KeyCache.begin(); it != m_KeyCache.end();)
            {
                if (it->second.expired())
                    it = m_KeyCache.erase(it);
                else
                    ++it;
            }

            for (auto it = m_GuidCache.begin(); it != m_GuidCache.end();)
            {
                if (it->second.expired())
                    it = m_GuidCache.erase(it);
                else
                    ++it;
            }
        }

        void GetCacheStats(size_t& keyCacheSize, size_t& guidCacheSize) const
        {
            std::shared_lock<std::shared_mutex> lock(m_Mutex);
            keyCacheSize = m_KeyCache.size();
            guidCacheSize = m_GuidCache.size();
        }

        void ClearCaches()
        {
            std::unique_lock<std::shared_mutex> lock(m_Mutex);
            m_KeyCache.clear();
            m_GuidCache.clear();
            m_FailedLoadRetryByKey.clear();
        }

        void SetRetryCooldown(std::chrono::milliseconds cooldown)
        {
            m_RetryCooldown = cooldown;
        }

    private:
        mutable std::shared_mutex m_Mutex;
        std::unordered_map<std::string, std::weak_ptr<Asset>> m_KeyCache;
        std::unordered_map<std::string, std::weak_ptr<Asset>> m_GuidCache;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_FailedLoadRetryByKey;
        AssetDatabase* m_Database = nullptr;
        std::chrono::milliseconds m_RetryCooldown{1000};
    };

    inline std::shared_ptr<Life::Asset> ResolveAssetByGuid(const std::string& guid)
    {
        auto* mgr = GetServices().TryGet<AssetManager>();
        return mgr ? mgr->GetByGuidAsset(guid) : nullptr;
    }
}