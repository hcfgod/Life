#pragma once

#include "Assets/AssetTypes.h"
#include "Core/Error.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetRegistryCacheEntry / Snapshot
    // Binary serialization format for fast asset database loading.
    // -----------------------------------------------------------------------------
    struct AssetRegistryCacheEntry
    {
        std::string Guid;
        std::string Key;
        std::string ResolvedPath;
        AssetType Type = AssetType::Unknown;
        std::string ImporterSettingsJson;
        std::vector<std::string> Dependencies;
        uint64_t SourceSizeBytes = 0;
        int64_t SourceLastWriteTimeTicks = 0;
        uint64_t ImporterSettingsHash64 = 0;
        uint32_t ImporterVersion = 0;
        uint8_t SourceKind = 0;
    };

    struct AssetRegistryCacheSnapshot
    {
        uint32_t DatabaseJsonVersion = 0;
        uint64_t SourceSizeBytes = 0;
        int64_t SourceLastWriteTimeTicks = 0;
        std::vector<AssetRegistryCacheEntry> Entries;
    };

    // -----------------------------------------------------------------------------
    // AssetRegistryCache
    // Fast binary cache for the asset database manifest.
    // -----------------------------------------------------------------------------
    class AssetRegistryCache final
    {
    public:
        static std::filesystem::path GetCacheFilePath(const std::filesystem::path& databaseFilePath);

        static Result<AssetRegistryCacheSnapshot> LoadFromFile(const std::filesystem::path& cacheFilePath,
                                                                uint32_t expectedDatabaseJsonVersion,
                                                                uint64_t expectedSourceSizeBytes,
                                                                int64_t expectedSourceLastWriteTimeTicks);

        static Result<void> SaveToFile(const std::filesystem::path& cacheFilePath,
                                        const AssetRegistryCacheSnapshot& snapshot);
    };
}
