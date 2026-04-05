#include "Assets/AssetImportPipeline.h"

#include "Assets/AssetImporterVersion.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetTypes.h"
#include "Assets/AssetUtils.h"
#include "Assets/GeneratedAssetRuntimeRegistry.h"

#include "Core/Concurrency/JobSystem.h"
#include "Core/Hash/XxHash64.h"
#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace Life::Assets::AssetImportPipeline
{
    namespace
    {
        bool EndsWith(const std::string& s, const std::string& suffix)
        {
            if (s.size() < suffix.size()) return false;
            return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        std::optional<AssetType> GuessTypeFromPath(const std::filesystem::path& path)
        {
            const std::string name = path.filename().string();
            const std::string ext = path.extension().string();

            if (EndsWith(name, ".scene.json")) return AssetType::Scene;
            if (EndsWith(name, ".prefab.json")) return AssetType::Prefab;
            if (EndsWith(name, ".tilemap.json")) return AssetType::Tilemap;
            if (EndsWith(name, ".tileset.json")) return AssetType::Tileset;
            if (EndsWith(name, ".tile.json")) return AssetType::Tile;
            if (EndsWith(name, ".tilepalette.json")) return AssetType::TilePalette;
            if (EndsWith(name, ".animationclip.json") || EndsWith(name, ".animation.json") || EndsWith(name, ".anim.json"))
                return AssetType::AnimationClip;
            if (EndsWith(name, ".animcontroller.json") || EndsWith(name, ".animatorcontroller.json"))
                return AssetType::AnimatorController;
            if (EndsWith(name, ".material.json")) return AssetType::Material;
            if (EndsWith(name, ".inputactions.json")) return AssetType::InputActions;
            if (EndsWith(name, ".audiomixer.json")) return AssetType::AudioMixer;
            if (ext == ".glsl") return AssetType::Shader;

            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" ||
                ext == ".hdr" || ext == ".psd" || ext == ".gif" || ext == ".ppm" || ext == ".pnm")
            {
                return AssetType::Texture2D;
            }

            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
            {
                return AssetType::AudioClip;
            }

            return std::nullopt;
        }

        int64_t GetLastWriteTimeTicksOrZero(const std::filesystem::path& path)
        {
            std::error_code ec;
            const auto t = std::filesystem::last_write_time(path, ec);
            if (ec) return 0;
            return static_cast<int64_t>(t.time_since_epoch().count());
        }

        uint64_t GetFileSizeOrZero(const std::filesystem::path& path)
        {
            std::error_code ec;
            const uint64_t size = std::filesystem::file_size(path, ec);
            return ec ? 0ull : size;
        }

        bool IsUpToDate(const AssetDatabase::Record& record,
                        const std::filesystem::path& filePath,
                        uint32_t currentImporterVersion)
        {
            const uint64_t sizeBytes = GetFileSizeOrZero(filePath);
            const int64_t lastWrite = GetLastWriteTimeTicksOrZero(filePath);
            if (sizeBytes == 0 || lastWrite == 0) return false;

            return record.SourceSizeBytes == sizeBytes &&
                   record.SourceLastWriteTimeTicks == lastWrite &&
                   record.ImporterVersion == currentImporterVersion;
        }

        void ReloadIfCached(const std::string& key, AssetManager& mgr)
        {
            // Try to find and reload a cached asset
            (void)key;
            (void)mgr;
            // TODO: Implement when AssetManager has GetCachedByKey
        }

        bool IsPathUnderRoot(const std::filesystem::path& candidatePath,
                             const std::filesystem::path& rootPath)
        {
            std::error_code ec;
            const std::filesystem::path candidate = std::filesystem::weakly_canonical(candidatePath, ec);
            if (ec) return false;
            ec.clear();
            const std::filesystem::path root = std::filesystem::weakly_canonical(rootPath, ec);
            if (ec) return false;
            ec.clear();
            const std::filesystem::path rel = std::filesystem::relative(candidate, root, ec);
            if (ec) return false;
            if (rel.empty()) return true;
            const std::string relText = rel.generic_string();
            return !(relText == ".." || relText.rfind("../", 0) == 0);
        }

        uint64_t ComputeImporterSettingsHash64(const nlohmann::json& importerSettings)
        {
            const std::string settingsText = importerSettings.dump();
            XxHash64::State hasher(0);
            hasher.Update(settingsText.data(), settingsText.size());
            return hasher.Digest();
        }

        struct ImportJob
        {
            std::string Key;
            AssetType Type = AssetType::Unknown;
        };

        Result<std::vector<ImportJob>> DiscoverKnownAssets(const std::filesystem::path& projectRoot)
        {
            std::vector<ImportJob> jobs;

            const std::filesystem::path assetsRoot = projectRoot / "Assets";
            if (!std::filesystem::exists(assetsRoot))
            {
                return Result<std::vector<ImportJob>>(ErrorCode::ResourceNotFound, "Assets/ directory not found: " + assetsRoot.string());
            }

            std::error_code ec;
            for (auto it = std::filesystem::recursive_directory_iterator(assetsRoot, ec);
                 it != std::filesystem::recursive_directory_iterator();
                 it.increment(ec))
            {
                if (ec) { ec.clear(); continue; }
                if (!it->is_regular_file(ec)) { ec.clear(); continue; }

                const auto& filePath = it->path();
                if (filePath.extension() == ".meta") continue;

                auto typeOpt = GuessTypeFromPath(filePath);
                if (!typeOpt.has_value()) continue;

                std::filesystem::path rel = std::filesystem::relative(filePath, projectRoot, ec);
                if (ec) { ec.clear(); continue; }

                ImportJob job;
                job.Key = rel.generic_string();
                job.Type = *typeOpt;
                jobs.push_back(std::move(job));
            }

            return jobs;
        }

        struct PreparedImport
        {
            enum class Status { NeedsCommit, UpToDate, MissingOnDisk, Error, Skipped };
            Status ImportStatus = Status::Skipped;
            std::string Key;
            std::string ErrorMessage;
            AssetDatabase::Record Record;
        };

        Result<AssetImportStatistics> ImportJobs(
            AssetDatabase& db,
            const std::filesystem::path& projectRoot,
            const std::vector<ImportJob>& jobs,
            bool changedOnly,
            bool includeDependents,
            const ParallelImportConfig& config)
        {
            AssetImportStatistics stats;
            stats.DiscoveredFiles = jobs.size();

            if (jobs.empty())
                return stats;

            const auto totalStart = std::chrono::steady_clock::now();

            db.EnsureLoaded();
            const auto existingRecords = db.GetAllRecords();

            std::unordered_map<std::string, AssetDatabase::Record> snapshotByKey;
            snapshotByKey.reserve(existingRecords.size());
            for (const auto& r : existingRecords)
            {
                if (!r.Key.empty())
                    snapshotByKey.emplace(r.Key, r);
            }

            const std::filesystem::path projectAssetsRoot = projectRoot / "Assets";

            std::vector<PreparedImport> prepared(jobs.size());

            const bool useParallel =
                !config.ForceSequential &&
                GetJobSystem().IsInitialized() &&
                jobs.size() > 1;

            stats.WorkerCount = useParallel ? GetJobSystem().GetWorkerCount() : 1;

            const auto prepStart = std::chrono::steady_clock::now();

            auto prepareOne = [&](size_t index)
            {
                const auto& job = jobs[index];
                auto& prep = prepared[index];
                prep.Key = job.Key;

                if (job.Key.empty())
                {
                    prep.ImportStatus = PreparedImport::Status::Skipped;
                    return;
                }

                const uint32_t importerVersion = GetCurrentAssetImporterVersion(job.Type);
                const std::filesystem::path abs = projectRoot / job.Key;

                if (!std::filesystem::exists(abs))
                {
                    prep.ImportStatus = PreparedImport::Status::MissingOnDisk;
                    return;
                }

                nlohmann::json settings = nlohmann::json::object();
                auto snapshotIt = snapshotByKey.find(job.Key);
                if (snapshotIt != snapshotByKey.end())
                    settings = snapshotIt->second.ImporterSettings;

                if (changedOnly && snapshotIt != snapshotByKey.end() &&
                    IsUpToDate(snapshotIt->second, abs, importerVersion))
                {
                    prep.ImportStatus = PreparedImport::Status::UpToDate;
                    return;
                }

                const auto resolvedPathResult = ResolveAssetKeyToPath(job.Key);
                if (resolvedPathResult.IsFailure())
                {
                    prep.ImportStatus = PreparedImport::Status::Error;
                    prep.ErrorMessage = resolvedPathResult.GetError().GetErrorMessage();
                    return;
                }
                const std::filesystem::path resolvedPath = resolvedPathResult.GetValue();

                if (!std::filesystem::exists(resolvedPath))
                {
                    prep.ImportStatus = PreparedImport::Status::Error;
                    prep.ErrorMessage = "Asset file not found: " + resolvedPath.string();
                    return;
                }

                if (!IsPathUnderRoot(resolvedPath, projectAssetsRoot))
                {
                    prep.ImportStatus = PreparedImport::Status::Error;
                    prep.ErrorMessage = "Resolved path outside Assets/: " + resolvedPath.string();
                    return;
                }

                std::error_code sizeEc;
                const uint64_t sizeBytes = std::filesystem::file_size(resolvedPath, sizeEc);
                const int64_t lastWriteTicks = GetLastWriteTimeTicksOrZero(resolvedPath);

                const auto guidResult = LoadOrCreateGuid(
                    resolvedPath.string(),
                    {{"key", job.Key}, {"type", ToString(job.Type)}});
                if (guidResult.IsFailure())
                {
                    prep.ImportStatus = PreparedImport::Status::Error;
                    prep.ErrorMessage = guidResult.GetError().GetErrorMessage();
                    return;
                }

                auto& record = prep.Record;
                record.Guid = guidResult.GetValue();
                record.Key = job.Key;
                record.ResolvedPath = resolvedPath.string();
                record.Type = job.Type;
                record.ImporterSettings = settings;
                record.SourceSizeBytes = sizeEc ? 0ull : sizeBytes;
                record.SourceLastWriteTimeTicks = lastWriteTicks;
                record.ImporterSettingsHash64 = ComputeImporterSettingsHash64(settings);
                record.ImporterVersion = importerVersion != 0u ? importerVersion : 1u;

                prep.ImportStatus = PreparedImport::Status::NeedsCommit;
            };

            if (useParallel)
            {
                LOG_CORE_INFO("AssetImportPipeline: parallel prep for {} jobs ({} workers)",
                             jobs.size(), GetJobSystem().GetWorkerCount());
                GetJobSystem().ParallelFor(0, jobs.size(), config.GrainSize, prepareOne);
            }
            else
            {
                for (size_t i = 0; i < jobs.size(); ++i)
                {
                    prepareOne(i);
                }
            }

            const auto prepEnd = std::chrono::steady_clock::now();
            stats.PrepMs = std::chrono::duration<double, std::milli>(prepEnd - prepStart).count();

            std::vector<AssetDatabase::Record> toCommit;
            toCommit.reserve(jobs.size());

            for (auto& prep : prepared)
            {
                switch (prep.ImportStatus)
                {
                    case PreparedImport::Status::MissingOnDisk: stats.MissingOnDisk++; break;
                    case PreparedImport::Status::UpToDate: stats.SkippedUpToDate++; break;
                    case PreparedImport::Status::Error:
                        stats.Errors++;
                        LOG_CORE_WARN("AssetImportPipeline: import prep failed key='{}': {}", prep.Key, prep.ErrorMessage);
                        break;
                    case PreparedImport::Status::NeedsCommit:
                        toCommit.push_back(std::move(prep.Record));
                        break;
                    case PreparedImport::Status::Skipped: break;
                }
            }

            std::sort(toCommit.begin(), toCommit.end(),
                      [](const AssetDatabase::Record& a, const AssetDatabase::Record& b)
                      { return a.Key < b.Key; });

            const auto commitStart = std::chrono::steady_clock::now();
            const auto committed = db.CommitRecordBatch(toCommit);

            std::vector<std::string> changedGuids;
            changedGuids.reserve(committed.size());
            for (const auto& r : committed)
            {
                stats.Imported++;
                stats.ImportedKeys.push_back(r.Key);
                changedGuids.push_back(r.Guid);
            }

            LOG_CORE_INFO("AssetImportPipeline: committed {} record(s) (discovered={}, skipped={}, missing={}, errors={})",
                         committed.size(), stats.DiscoveredFiles, stats.SkippedUpToDate,
                         stats.MissingOnDisk, stats.Errors);

            const auto commitEnd = std::chrono::steady_clock::now();
            stats.CommitMs = std::chrono::duration<double, std::milli>(commitEnd - commitStart).count();

            // Cascade to dependents
            const auto cascadeStart = std::chrono::steady_clock::now();
            if (!includeDependents || changedGuids.empty())
            {
                stats.TotalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - totalStart).count();
                return stats;
            }

            std::deque<std::string> queue;
            std::unordered_set<std::string> visited;
            visited.reserve(512);

            for (const auto& guid : changedGuids)
            {
                if (!guid.empty())
                    queue.push_back(guid);
            }

            while (!queue.empty())
            {
                const std::string currentGuid = queue.front();
                queue.pop_front();

                if (!visited.emplace(currentGuid).second)
                    continue;

                const auto dependents = db.GetDependentsOf(currentGuid);
                for (const auto& dep : dependents)
                {
                    if (dep.SourceKind == AssetSourceKind::Generated)
                    {
                        if (GeneratedAssetRuntimeRegistry::GetInstance().Reload(dep.Key))
                        {
                            stats.Imported++;
                            stats.ImportedKeys.push_back(dep.Key);
                            queue.push_back(dep.Guid);
                        }
                        continue;
                    }

                    const auto reimport = db.ImportOrUpdate(
                        dep.Key,
                        dep.Type,
                        dep.ImporterSettings,
                        GetCurrentAssetImporterVersion(dep.Type));
                    if (reimport.IsSuccess())
                    {
                        stats.Imported++;
                        stats.ImportedKeys.push_back(dep.Key);
                        queue.push_back(reimport.GetValue().Guid.empty() ? dep.Guid : reimport.GetValue().Guid);
                    }
                }
            }

            const auto cascadeEnd = std::chrono::steady_clock::now();
            stats.CascadeMs = std::chrono::duration<double, std::milli>(cascadeEnd - cascadeStart).count();
            stats.TotalMs = std::chrono::duration<double, std::milli>(cascadeEnd - totalStart).count();

            return stats;
        }

        size_t PruneMissingRecords(AssetDatabase& db)
        {
            size_t removedCount = 0;
            const auto records = db.GetAllRecords();
            for (const auto& record : records)
            {
                if (record.SourceKind == AssetSourceKind::Generated)
                    continue;

                std::filesystem::path resolvedPath;
                if (!record.ResolvedPath.empty())
                {
                    resolvedPath = std::filesystem::path(record.ResolvedPath);
                }
                else if (!record.Key.empty())
                {
                    const auto resolvedResult = ResolveAssetKeyToPath(record.Key);
                    if (resolvedResult.IsSuccess())
                        resolvedPath = resolvedResult.GetValue();
                }

                std::error_code ec;
                const bool missingOnDisk = resolvedPath.empty() || !std::filesystem::exists(resolvedPath, ec);
                if (!missingOnDisk)
                    continue;

                Result<void> removeResult(ErrorCode::InvalidState, "missing record identity");
                if (!record.Guid.empty())
                    removeResult = db.RemoveByGuid(record.Guid);
                else if (!record.Key.empty())
                    removeResult = db.RemoveByKey(record.Key);

                if (removeResult.IsSuccess())
                    ++removedCount;
            }
            return removedCount;
        }
    }

    Result<AssetImportStatistics> ReimportAll(bool includeDependents, const ParallelImportConfig& config)
    {
        auto* db = GetServices().TryGet<AssetDatabase>();
        if (!db)
        {
            return Result<AssetImportStatistics>(ErrorCode::InvalidState, "AssetDatabase service not available");
        }

        const auto rootResult = FindProjectRootFromWorkingDirectory();
        if (rootResult.IsFailure())
        {
            return Result<AssetImportStatistics>(rootResult.GetError());
        }

        const std::filesystem::path projectRoot = rootResult.GetValue();
        const auto discoveryStart = std::chrono::steady_clock::now();
        const auto jobsResult = DiscoverKnownAssets(projectRoot);
        const auto discoveryEnd = std::chrono::steady_clock::now();
        if (jobsResult.IsFailure())
        {
            return Result<AssetImportStatistics>(jobsResult.GetError());
        }

        auto importResult = ImportJobs(*db, projectRoot, jobsResult.GetValue(), false, includeDependents, config);
        if (importResult.IsFailure())
            return importResult;

        importResult.GetValue().DiscoveryMs =
            std::chrono::duration<double, std::milli>(discoveryEnd - discoveryStart).count();
        importResult.GetValue().TotalMs += importResult.GetValue().DiscoveryMs;

        const size_t removed = PruneMissingRecords(*db);
        if (removed > 0)
            LOG_CORE_INFO("AssetImportPipeline: pruned {} missing AssetDatabase record(s)", removed);

        return importResult;
    }

    Result<AssetImportStatistics> ReimportChanged(bool includeDependents, const ParallelImportConfig& config)
    {
        auto* db = GetServices().TryGet<AssetDatabase>();
        if (!db)
        {
            return Result<AssetImportStatistics>(ErrorCode::InvalidState, "AssetDatabase service not available");
        }

        const auto rootResult = FindProjectRootFromWorkingDirectory();
        if (rootResult.IsFailure())
        {
            return Result<AssetImportStatistics>(rootResult.GetError());
        }

        const std::filesystem::path projectRoot = rootResult.GetValue();
        const auto discoveryStart = std::chrono::steady_clock::now();
        const auto jobsResult = DiscoverKnownAssets(projectRoot);
        const auto discoveryEnd = std::chrono::steady_clock::now();
        if (jobsResult.IsFailure())
        {
            return Result<AssetImportStatistics>(jobsResult.GetError());
        }

        auto importResult = ImportJobs(*db, projectRoot, jobsResult.GetValue(), true, includeDependents, config);
        if (importResult.IsFailure())
            return importResult;

        importResult.GetValue().DiscoveryMs =
            std::chrono::duration<double, std::milli>(discoveryEnd - discoveryStart).count();
        importResult.GetValue().TotalMs += importResult.GetValue().DiscoveryMs;

        const size_t removed = PruneMissingRecords(*db);
        if (removed > 0)
            LOG_CORE_INFO("AssetImportPipeline: pruned {} missing AssetDatabase record(s)", removed);

        return importResult;
    }

    Result<std::vector<AssetDatabaseValidationIssue>> ValidateAssetDatabase()
    {
        auto* db = GetServices().TryGet<AssetDatabase>();
        if (!db)
        {
            return Result<std::vector<AssetDatabaseValidationIssue>>(ErrorCode::InvalidState, "AssetDatabase service not available");
        }

        std::vector<AssetDatabaseValidationIssue> issues;
        const auto records = db->GetAllRecords();

        std::unordered_map<std::string, const AssetDatabase::Record*> recordsByGuid;
        recordsByGuid.reserve(records.size());

        std::unordered_map<std::string, std::string> firstKeyByGuid;
        firstKeyByGuid.reserve(records.size());

        for (const auto& r : records)
        {
            if (r.Guid.empty() || r.Key.empty()) continue;
            recordsByGuid.emplace(r.Guid, &r);
        }

        for (const auto& r : records)
        {
            if (r.Guid.empty() || r.Key.empty()) continue;

            std::error_code ec;
            if (!r.ResolvedPath.empty() && !std::filesystem::exists(std::filesystem::path(r.ResolvedPath), ec))
            {
                AssetDatabaseValidationIssue issue;
                issue.IssueType = AssetDatabaseValidationIssue::Type::MissingFileForRecord;
                issue.Guid = r.Guid;
                issue.Key = r.Key;
                issue.ResolvedPath = r.ResolvedPath;
                issue.Message = "Missing file on disk for record";
                issues.push_back(std::move(issue));
            }

            if (auto it = firstKeyByGuid.find(r.Guid); it != firstKeyByGuid.end())
            {
                if (it->second != r.Key)
                {
                    AssetDatabaseValidationIssue issue;
                    issue.IssueType = AssetDatabaseValidationIssue::Type::DuplicateGuidForDifferentKeys;
                    issue.Guid = r.Guid;
                    issue.Key = r.Key;
                    issue.Message = "Duplicate GUID mapped to multiple keys ('" + it->second + "' and '" + r.Key + "')";
                    issues.push_back(std::move(issue));
                }
            }
            else
            {
                firstKeyByGuid.emplace(r.Guid, r.Key);
            }
        }

        for (const auto& r : records)
        {
            if (r.Guid.empty() || r.Key.empty()) continue;

            for (const auto& dependencyGuid : r.Dependencies)
            {
                if (dependencyGuid.empty()) continue;

                if (dependencyGuid == r.Guid)
                {
                    AssetDatabaseValidationIssue issue;
                    issue.IssueType = AssetDatabaseValidationIssue::Type::SelfDependency;
                    issue.Guid = r.Guid;
                    issue.Key = r.Key;
                    issue.ResolvedPath = r.ResolvedPath;
                    issue.Message = "Asset depends on itself";
                    issues.push_back(std::move(issue));
                    continue;
                }

                if (recordsByGuid.find(dependencyGuid) == recordsByGuid.end())
                {
                    AssetDatabaseValidationIssue issue;
                    issue.IssueType = AssetDatabaseValidationIssue::Type::MissingDependencyRecord;
                    issue.Guid = r.Guid;
                    issue.Key = r.Key;
                    issue.ResolvedPath = r.ResolvedPath;
                    issue.Message = "Missing dependency GUID record '" + dependencyGuid + "'";
                    issues.push_back(std::move(issue));
                }
            }
        }

        return issues;
    }
}
