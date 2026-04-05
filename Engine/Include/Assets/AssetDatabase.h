#pragma once

#include "Assets/AssetTypes.h"
#include "Core/Error.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetSourceKind
    // -----------------------------------------------------------------------------
    enum class AssetSourceKind : uint8_t
    {
        File = 0,
        Generated = 1
    };

    [[nodiscard]] inline const char* ToString(AssetSourceKind kind)
    {
        switch (kind)
        {
            case AssetSourceKind::Generated: return "Generated";
            case AssetSourceKind::File:
            default: return "File";
        }
    }

    [[nodiscard]] inline AssetSourceKind AssetSourceKindFromString(const std::string& s)
    {
        if (s == "Generated") return AssetSourceKind::Generated;
        return AssetSourceKind::File;
    }

    // -----------------------------------------------------------------------------
    // AssetDatabase
    // Persistent manifest mapping GUIDs to keys/paths, asset types, importer settings,
    // and dependencies. Thread-safe.
    //
    // Host-owned service — create via ApplicationHost, register in ServiceRegistry.
    // -----------------------------------------------------------------------------
    class AssetDatabase final
    {
    public:
        struct Record
        {
            std::string Guid;
            std::string Key;
            std::string ResolvedPath;
            AssetType Type = AssetType::Unknown;
            nlohmann::json ImporterSettings = nlohmann::json::object();
            std::vector<std::string> Dependencies;
            uint64_t SourceSizeBytes = 0;
            int64_t SourceLastWriteTimeTicks = 0;
            uint64_t ImporterSettingsHash64 = 0;
            uint32_t ImporterVersion = 0;
            AssetSourceKind SourceKind = AssetSourceKind::File;
        };

        struct CacheTelemetry
        {
            uint64_t CacheHits = 0;
            uint64_t CacheMisses = 0;
        };

        AssetDatabase() = default;
        ~AssetDatabase() = default;

        AssetDatabase(const AssetDatabase&) = delete;
        AssetDatabase& operator=(const AssetDatabase&) = delete;

        void EnsureLoaded();
        void Reset();

        Result<Record> FindByGuid(const std::string& guid);
        Result<Record> FindByKey(const std::string& key);

        Result<Record> ImportOrUpdate(const std::string& key,
                                       AssetType type,
                                       const nlohmann::json& importerSettings = nlohmann::json::object(),
                                       uint32_t importerVersion = 0);

        Result<void> SetDependencies(const std::string& guid, const std::vector<std::string>& dependencies);

        std::vector<Record> GetDependentsOf(const std::string& guid);
        std::vector<Record> GetAllRecords();

        std::vector<Record> CommitRecordBatch(std::vector<Record>& records);

        Result<Record> RegisterGeneratedAsset(const std::string& guid,
                                               const std::string& virtualKey,
                                               AssetType type,
                                               const nlohmann::json& importerSettings = nlohmann::json::object(),
                                               uint32_t importerVersion = 0);

        Result<void> RemoveByGuid(const std::string& guid);
        Result<void> RemoveByKey(const std::string& key);

        size_t GetRecordCount() const;
        uint64_t GetRevision() const;
        CacheTelemetry GetCacheTelemetry() const;

    private:
        Result<std::filesystem::path> GetDatabaseFilePathLocked();
        Result<void> LoadFromDiskLocked();
        Result<void> SaveToDiskLocked();
        void RebuildDependentsIndexLocked();

        static nlohmann::json RecordToJson(const Record& r);
        static Result<Record> RecordFromJson(const nlohmann::json& j);

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, Record> m_ByGuid;
        std::unordered_map<std::string, std::string> m_GuidByKey;
        std::unordered_map<std::string, std::vector<std::string>> m_DependentsByGuid;
        bool m_Loaded = false;
        uint64_t m_Revision = 0;
        uint64_t m_CacheHits = 0;
        uint64_t m_CacheMisses = 0;
    };
}
