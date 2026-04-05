#pragma once

#include "Assets/AssetDatabase.h"
#include "Core/Error.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Life::Assets::AssetImportPipeline
{
    // -----------------------------------------------------------------------------
    // AssetImportStatistics
    // Returned from reimport operations to report what happened.
    // -----------------------------------------------------------------------------
    struct AssetImportStatistics
    {
        size_t DiscoveredFiles = 0;
        size_t Imported = 0;
        size_t SkippedUpToDate = 0;
        size_t MissingOnDisk = 0;
        size_t Errors = 0;
        size_t WorkerCount = 1;
        std::vector<std::string> ImportedKeys;
        double DiscoveryMs = 0.0;
        double PrepMs = 0.0;
        double CommitMs = 0.0;
        double CascadeMs = 0.0;
        double TotalMs = 0.0;
    };

    // -----------------------------------------------------------------------------
    // AssetDatabaseValidationIssue
    // Reported by ValidateAssetDatabase() for integrity checks.
    // -----------------------------------------------------------------------------
    struct AssetDatabaseValidationIssue
    {
        enum class Type
        {
            MissingFileForRecord,
            DuplicateGuidForDifferentKeys,
            MissingDependencyRecord,
            SelfDependency,
            CyclicDependency
        };

        Type IssueType = Type::MissingFileForRecord;
        std::string Guid;
        std::string Key;
        std::string ResolvedPath;
        std::string Message;
    };

    // -----------------------------------------------------------------------------
    // ParallelImportConfig
    // Controls parallel import behavior.
    // -----------------------------------------------------------------------------
    struct ParallelImportConfig
    {
        bool ForceSequential = false;
        size_t GrainSize = 16;
    };

    // -----------------------------------------------------------------------------
    // Public API
    // All functions obtain the AssetDatabase from the ServiceRegistry.
    // -----------------------------------------------------------------------------
    Result<AssetImportStatistics> ReimportAll(bool includeDependents = true,
                                              const ParallelImportConfig& config = {});

    Result<AssetImportStatistics> ReimportChanged(bool includeDependents = true,
                                                   const ParallelImportConfig& config = {});

    Result<std::vector<AssetDatabaseValidationIssue>> ValidateAssetDatabase();
}
