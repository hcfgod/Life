#include "Assets/AssetHotReloadManager.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetImporterVersion.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPaths.h"
#include "Assets/GeneratedAssetRuntimeRegistry.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Life::Assets
{
    namespace
    {
        bool ReloadCachedRecord(const AssetDatabase::Record& record, AssetManager* assetManager)
        {
            if (assetManager == nullptr)
                return false;

            if (assetManager->GetCachedByKeyAsset(record.Key) == nullptr)
            {
                LOG_CORE_INFO("AssetHotReload: reimported '{}' with no live cached asset to reload.", record.Key);
                return false;
            }

            if (!assetManager->ReloadCachedAssetByKey(record.Key))
            {
                LOG_CORE_WARN("AssetHotReload: cached reload failed for '{}'.", record.Key);
                return false;
            }

            return true;
        }

        bool ReloadGeneratedRecord(const AssetDatabase::Record& record, AssetManager* assetManager)
        {
            bool generatedReloaded = GeneratedAssetRuntimeRegistry::GetInstance().Reload(record.Key);
            const bool cachedReloaded = ReloadCachedRecord(record, assetManager);
            return generatedReloaded || cachedReloaded;
        }

        bool ReimportFileRecord(AssetDatabase& db,
                                const AssetDatabase::Record& record,
                                AssetManager* assetManager,
                                AssetDatabase::Record& updatedRecord)
        {
            const auto reimportResult = db.ImportOrUpdate(
                record.Key,
                record.Type,
                record.ImporterSettings,
                GetCurrentAssetImporterVersion(record.Type));
            if (reimportResult.IsFailure())
            {
                LOG_CORE_WARN("AssetHotReload: reimport failed for '{}': {}", record.Key, reimportResult.GetError().GetErrorMessage());
                return false;
            }

            updatedRecord = reimportResult.GetValue();
            (void)ReloadCachedRecord(updatedRecord, assetManager);
            return true;
        }
    }

    void AssetHotReloadManager::Enable(bool enable)
    {
        m_Enabled.store(enable, std::memory_order_relaxed);
        if (!enable)
        {
            Shutdown();
        }
    }

    void AssetHotReloadManager::WatchKey(const std::string& key)
    {
        if (!IsEnabled() || key.empty())
        {
            return;
        }

        // Shipping/bundle mode: do not start file watchers.
        {
            auto* bundle = GetServices().TryGet<AssetBundle>();
            if (bundle && bundle->IsEnabled() && bundle->IsLoaded())
            {
                return;
            }
        }

        auto* db = GetServices().TryGet<AssetDatabase>();
        if (!db) return;

        auto recordResult = db->FindByKey(key);
        if (recordResult.IsFailure())
        {
            return;
        }

        const auto& record = recordResult.GetValue();

        if (record.SourceKind == AssetSourceKind::Generated || record.ResolvedPath.empty())
        {
            return;
        }

        bool shouldStartWatcher = false;
        bool shouldStartReloadThread = false;
        std::filesystem::path assetsRoot;

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (!m_TreeWatcher)
            {
                const auto rootResult = FindProjectRootFromWorkingDirectory();
                if (rootResult.IsFailure())
                {
                    LOG_CORE_ERROR("AssetHotReload: cannot start tree watcher: {}", rootResult.GetError().GetErrorMessage());
                }
                else
                {
                    assetsRoot = rootResult.GetValue() / "Assets";
                    std::error_code ec;
                    if (std::filesystem::exists(assetsRoot, ec) && std::filesystem::is_directory(assetsRoot, ec))
                    {
                        m_TreeWatcher = std::make_unique<AssetTreeWatcher>();
                        shouldStartWatcher = true;
                    }
                    else
                    {
                        static bool s_WarnedMissingAssets = false;
                        if (!s_WarnedMissingAssets)
                        {
                            s_WarnedMissingAssets = true;
                            LOG_CORE_WARN("AssetHotReload: Assets directory not found; hot reload will remain disabled. AssetsRoot='{}'", assetsRoot.string());
                        }
                    }
                }
            }

            if (!m_ReloadThreadRunning)
            {
                m_ReloadThreadRunning = true;
                shouldStartReloadThread = true;
            }

            if (m_ByResolvedPath.find(record.ResolvedPath) == m_ByResolvedPath.end())
            {
                WatchEntry entry;
                entry.key = record.Key;
                entry.guid = record.Guid;
                entry.resolvedPath = record.ResolvedPath;
                entry.type = record.Type;
                entry.importerSettings = record.ImporterSettings;
                m_ByResolvedPath.emplace(entry.resolvedPath, std::move(entry));
            }
        }

        if (shouldStartWatcher && m_TreeWatcher)
        {
            m_TreeWatcher->Start(assetsRoot, [this](const std::filesystem::path& changed) {
                OnFileChanged(changed);
            });
        }

        if (shouldStartReloadThread)
        {
            m_ReloadThread = std::thread(&AssetHotReloadManager::ReloadThreadMain, this);
        }
    }

    void AssetHotReloadManager::RequestReload(const std::string& key, const std::string& guid)
    {
        if (!IsEnabled() || key.empty())
            return;

        bool shouldStartReloadThread = false;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (!m_ReloadThreadRunning)
            {
                m_ReloadThreadRunning = true;
                shouldStartReloadThread = true;
            }

            EnqueueReloadLocked(key, guid);
        }

        if (shouldStartReloadThread)
            m_ReloadThread = std::thread(&AssetHotReloadManager::ReloadThreadMain, this);
    }

    void AssetHotReloadManager::Pump()
    {
        if (!IsEnabled())
            return;

        std::vector<PendingReload> ready;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_ReadyReloads.empty())
                return;

            ready.assign(m_ReadyReloads.begin(), m_ReadyReloads.end());
            m_ReadyReloads.clear();
        }

        auto* db = GetServices().TryGet<AssetDatabase>();
        if (!db)
            return;

        auto* assetManager = GetServices().TryGet<AssetManager>();

        std::deque<AssetDatabase::Record> queue;
        std::unordered_set<std::string> visitedGuids;
        visitedGuids.reserve(256);

        for (const auto& job : ready)
        {
            auto recordResult = db->FindByKey(job.key);
            if (recordResult.IsFailure())
                continue;

            const auto& record = recordResult.GetValue();
            if (record.SourceKind == AssetSourceKind::Generated)
            {
                const bool reloaded = ReloadGeneratedRecord(record, assetManager);
                if (!reloaded)
                {
                    LOG_CORE_WARN("AssetHotReload: generated reload failed for '{}'", record.Key);
                    continue;
                }

                queue.push_back(record);
                continue;
            }

            AssetDatabase::Record updatedRecord;
            LOG_CORE_INFO("AssetHotReload: reimporting key='{}'", record.Key);
            if (ReimportFileRecord(*db, record, assetManager, updatedRecord))
                queue.push_back(std::move(updatedRecord));
        }

        while (!queue.empty())
        {
            const auto current = queue.front();
            queue.pop_front();

            if (current.Guid.empty() || current.Key.empty())
                continue;

            if (!visitedGuids.emplace(current.Guid).second)
                continue;

            const auto dependents = db->GetDependentsOf(current.Guid);
            for (const auto& dep : dependents)
            {
                bool reloaded = false;
                if (dep.SourceKind == AssetSourceKind::Generated)
                {
                    reloaded = ReloadGeneratedRecord(dep, assetManager);
                    if (reloaded)
                        queue.push_back(dep);
                }
                else
                {
                    AssetDatabase::Record updatedDependent;
                    reloaded = ReimportFileRecord(*db, dep, assetManager, updatedDependent);
                    if (reloaded)
                        queue.push_back(std::move(updatedDependent));
                }

                LOG_CORE_INFO("AssetHotReload: reload key='{}' result={}", dep.Key, reloaded ? "success" : "no-op");
            }
        }
    }

    void AssetHotReloadManager::Shutdown()
    {
        std::unique_ptr<AssetTreeWatcher> watcherToStop;
        std::thread reloadThreadToJoin;

        {
            std::lock_guard<std::mutex> lock(m_Mutex);

            m_ByResolvedPath.clear();
            m_PendingByKey.clear();
            m_ReadyReloads.clear();

            watcherToStop = std::move(m_TreeWatcher);

            if (m_ReloadThreadRunning)
            {
                m_ReloadThreadStop = true;
                m_ReloadCv.notify_all();
                reloadThreadToJoin = std::move(m_ReloadThread);
                m_ReloadThreadRunning = false;
            }
        }

        if (watcherToStop)
        {
            watcherToStop->Stop();
        }

        if (reloadThreadToJoin.joinable())
        {
            reloadThreadToJoin.join();
        }

        m_ReloadThreadStop = false;
    }

    void AssetHotReloadManager::EnsureWatcherRunningLocked()
    {
        // Deprecated: watcher startup must occur outside locks to avoid deadlocks.
    }

    void AssetHotReloadManager::OnFileChanged(const std::filesystem::path& changedPath)
    {
        if (!IsEnabled())
        {
            return;
        }

        std::string key;
        std::string guid;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            const auto it = m_ByResolvedPath.find(changedPath.string());
            if (it == m_ByResolvedPath.end())
            {
                return;
            }
            key = it->second.key;
            guid = it->second.guid;

            EnqueueReloadLocked(key, guid);
        }

        LOG_CORE_INFO("AssetHotReload: change detected '{}', queued reload key='{}'", changedPath.string(), key);
    }

    void AssetHotReloadManager::EnqueueReloadLocked(const std::string& key, const std::string& guid)
    {
        if (m_DebounceWindow.count() <= 0)
        {
            PendingReload pending;
            pending.key = key;
            pending.guid = guid;
            pending.generation = 1;
            pending.dueTime = std::chrono::steady_clock::now();

            for (PendingReload& ready : m_ReadyReloads)
            {
                if (ready.key == key)
                {
                    ready = std::move(pending);
                    return;
                }
            }

            m_ReadyReloads.push_back(std::move(pending));
            return;
        }

        auto& pending = m_PendingByKey[key];
        pending.key = key;
        pending.guid = guid;
        pending.generation++;
        pending.dueTime = std::chrono::steady_clock::now() + m_DebounceWindow;
        m_ReloadCv.notify_all();
    }

    void AssetHotReloadManager::ReloadThreadMain()
    {
        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_ReloadCv.wait(lock, [this] {
                    return m_ReloadThreadStop || !m_PendingByKey.empty();
                });

                if (m_ReloadThreadStop)
                    return;

                auto now = std::chrono::steady_clock::now();
                auto nextDue = now + std::chrono::hours(24);
                for (const auto& [key, pending] : m_PendingByKey)
                {
                    (void)key;
                    if (pending.dueTime < nextDue)
                        nextDue = pending.dueTime;
                }

                if (nextDue > now)
                {
                    m_ReloadCv.wait_until(lock, nextDue, [this] { return m_ReloadThreadStop; });
                    if (m_ReloadThreadStop)
                        return;
                }

                now = std::chrono::steady_clock::now();
                for (auto it = m_PendingByKey.begin(); it != m_PendingByKey.end();)
                {
                    if (it->second.dueTime <= now)
                    {
                        m_ReadyReloads.push_back(it->second);
                        it = m_PendingByKey.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }
    }
}
