#include "Assets/AssetBundleBuilder.h"

#include "Assets/AssetDatabase.h"
#include "Assets/AssetImporterVersion.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetTypes.h"
#include "Assets/AssetUtils.h"
#include "Assets/ImageDecode.h"
#include "Assets/ShaderStageParsing.h"
#include "Assets/TextureSpecificationJson.h"

#include "Core/Hash/XxHash64.h"
#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <array>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Life::Assets
{
    using json = nlohmann::json;

    static constexpr uint32_t kAssetBundleCacheVersion = 1;
    static constexpr uint32_t kAssetCookingVersion = 1;

    static const char* ToString(AssetBundleCompression compression)
    {
        switch (compression)
        {
        case AssetBundleCompression::None: return "None";
        case AssetBundleCompression::Zstd: return "Zstd";
        default: return "None";
        }
    }

    static const char* ToString(AssetBundlePayloadFormat format)
    {
        switch (format)
        {
        case AssetBundlePayloadFormat::Raw: return "Raw";
        case AssetBundlePayloadFormat::CookedTexture2D: return "CookedTexture2D";
        case AssetBundlePayloadFormat::CookedShaderStages: return "CookedShaderStages";
        default: return "Raw";
        }
    }

    static bool EndsWith(const std::string& s, const std::string& suffix)
    {
        if (s.size() < suffix.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static std::optional<AssetType> GuessTypeFromPath(const std::filesystem::path& path)
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

    static Result<std::vector<uint8_t>> ReadAllBytes(const std::filesystem::path& file)
    {
        std::ifstream in(file, std::ios::in | std::ios::binary | std::ios::ate);
        if (!in.is_open())
        {
            return Result<std::vector<uint8_t>>(ErrorCode::FileNotFound, "Failed to open file: " + file.string());
        }

        const std::streamsize size = in.tellg();
        if (size <= 0)
        {
            return Result<std::vector<uint8_t>>(ErrorCode::FileCorrupted, "File is empty: " + file.string());
        }

        in.seekg(0, std::ios::beg);
        std::vector<uint8_t> bytes;
        bytes.resize(static_cast<size_t>(size));
        in.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!in.good())
        {
            return Result<std::vector<uint8_t>>(ErrorCode::FileCorrupted, "Failed to read file: " + file.string());
        }

        return bytes;
    }

    static Result<void> AtomicWriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
    {
        try
        {
            if (path.has_parent_path())
            {
                std::filesystem::create_directories(path.parent_path());
            }

            const std::filesystem::path tmp = path.string() + ".tmp";
            {
                std::ofstream out(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
                if (!out.is_open())
                {
                    return Result<void>(ErrorCode::FileAccessDenied, "Failed to write temp file: " + tmp.string());
                }
                out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                out.flush();
            }

            std::error_code ec;
            std::filesystem::rename(tmp, path, ec);
            if (ec)
            {
                ec.clear();
                std::filesystem::remove(path, ec);
                ec.clear();
                std::filesystem::rename(tmp, path, ec);
                if (ec)
                {
                    return Result<void>(ErrorCode::FileAccessDenied, "Failed to replace file: " + ec.message());
                }
            }
        }
        catch (const std::exception& e)
        {
            return Result<void>(ErrorCode::FileAccessDenied, std::string("AtomicWriteFile failed: ") + e.what());
        }

        return Result<void>();
    }

    Result<void> AssetBundleBuilder::BuildProjectAssetBundle()
    {
        return BuildProjectAssetBundle(Settings{});
    }

    Result<void> AssetBundleBuilder::BuildProjectAssetBundle(Settings settings)
    {
        const auto rootResult = FindProjectRootFromWorkingDirectory();
        if (rootResult.IsFailure())
        {
            return Result<void>(rootResult.GetError());
        }

        const std::filesystem::path root = rootResult.GetValue();
        const std::filesystem::path outDir = root / "Build" / "AssetBundle";
        return BuildAtOutputDirectory(outDir, settings);
    }

    Result<void> AssetBundleBuilder::BuildAssetBundleToDirectory(const std::filesystem::path& outputDirectory)
    {
        return BuildAssetBundleToDirectory(outputDirectory, Settings{});
    }

    Result<void> AssetBundleBuilder::BuildAssetBundleToDirectory(const std::filesystem::path& outputDirectory, Settings settings)
    {
        return BuildAtOutputDirectory(outputDirectory, settings);
    }

    Result<void> AssetBundleBuilder::BuildAtOutputDirectory(const std::filesystem::path& outputDirectory, Settings settings)
    {
        if (outputDirectory.empty())
        {
            return Result<void>(ErrorCode::InvalidArgument, "AssetBundleBuilder: outputDirectory is empty");
        }

        const auto rootResult = FindProjectRootFromWorkingDirectory();
        if (rootResult.IsFailure())
        {
            return Result<void>(rootResult.GetError());
        }

        const std::filesystem::path projectRoot = rootResult.GetValue();
        const std::filesystem::path assetsRoot = projectRoot / "Assets";
        if (!std::filesystem::exists(assetsRoot))
        {
            return Result<void>(ErrorCode::ResourceNotFound, "AssetBundleBuilder: Assets/ directory not found: " + assetsRoot.string());
        }

        // Get AssetDatabase from ServiceRegistry
        auto* db = GetServices().TryGet<AssetDatabase>();
        if (!db)
        {
            return Result<void>(ErrorCode::InvalidState, "AssetBundleBuilder: AssetDatabase service not available");
        }

        // Discover and import all known asset types under Assets/.
        size_t discovered = 0;
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
            const std::string key = rel.generic_string();

            nlohmann::json importerSettings = nlohmann::json::object();
            const auto existingRecord = db->FindByKey(key);
            if (existingRecord.IsSuccess())
            {
                importerSettings = existingRecord.GetValue().ImporterSettings;
            }
            (void)db->ImportOrUpdate(key, *typeOpt, importerSettings, GetCurrentAssetImporterVersion(*typeOpt));
            discovered++;
        }

        auto records = db->GetAllRecords();
        if (records.empty())
        {
            return Result<void>(ErrorCode::ResourceNotFound, "AssetBundleBuilder: AssetDatabase has no records");
        }

        struct OutEntry
        {
            std::string Guid;
            std::string Key;
            AssetType Type = AssetType::Unknown;
            AssetBundlePayloadFormat PayloadFormat = AssetBundlePayloadFormat::Raw;
            AssetBundleCompression Compression = AssetBundleCompression::None;
            uint64_t Offset = 0;
            uint64_t Size = 0;
            uint64_t UncompressedSize = 0;
            uint64_t ContentHash64 = 0;
        };

        std::vector<uint8_t> data;
        data.reserve(8 * 1024 * 1024);

        std::vector<OutEntry> entries;
        entries.reserve(records.size());

        std::unordered_map<std::string, std::string> firstKeyByGuid;
        firstKeyByGuid.reserve(records.size());

        for (const auto& r : records)
        {
            if (r.Guid.empty() || r.Key.empty() || r.ResolvedPath.empty()) continue;

            if (const auto it = firstKeyByGuid.find(r.Guid); it != firstKeyByGuid.end())
            {
                if (it->second != r.Key)
                {
                    return Result<void>(ErrorCode::ResourceCorrupted,
                        "AssetBundleBuilder: duplicate GUID detected (guid=" + r.Guid + ") for keys '" + it->second + "' and '" + r.Key + "'");
                }
            }
            else
            {
                firstKeyByGuid.emplace(r.Guid, r.Key);
            }

            const std::filesystem::path filePath = r.ResolvedPath;
            const auto bytesResult = ReadAllBytes(filePath);
            if (bytesResult.IsFailure())
            {
                LOG_CORE_WARN("AssetBundleBuilder: skipping '{}' (read failed): {}", r.Key, bytesResult.GetError().GetErrorMessage());
                continue;
            }

            const auto& bytes = bytesResult.GetValue();
            std::vector<uint8_t> storedBytes = bytes;

            // Compute content hash
            XxHash64::State hasher(0);
            hasher.Update(bytes.data(), bytes.size());
            const std::string settingsText = r.ImporterSettings.dump();
            hasher.Update(settingsText.data(), settingsText.size());
            hasher.Update(&kAssetCookingVersion, sizeof(kAssetCookingVersion));
            const uint64_t contentHash64 = hasher.Digest();

            OutEntry e;
            e.Guid = r.Guid;
            e.Key = r.Key;
            e.Type = r.Type;
            e.PayloadFormat = AssetBundlePayloadFormat::Raw;
            e.ContentHash64 = contentHash64;
            e.Compression = AssetBundleCompression::None;
            e.UncompressedSize = static_cast<uint64_t>(storedBytes.size());
            e.Offset = static_cast<uint64_t>(data.size());
            e.Size = static_cast<uint64_t>(storedBytes.size());

            data.insert(data.end(), storedBytes.begin(), storedBytes.end());
            entries.push_back(std::move(e));
        }

        // Write data file
        std::filesystem::create_directories(outputDirectory, ec);
        const std::filesystem::path dataPath = outputDirectory / "AssetBundleData.bin";
        {
            auto writeResult = AtomicWriteFile(dataPath, data);
            if (writeResult.IsFailure())
            {
                return writeResult;
            }
        }

        // Write manifest
        json manifest;
        manifest["version"] = 2;
        manifest["dataFile"] = "AssetBundleData.bin";
        manifest["entries"] = json::array();

        for (const auto& e : entries)
        {
            json entry;
            entry["guid"] = e.Guid;
            entry["key"] = e.Key;
            entry["type"] = Life::Assets::ToString(e.Type);
            entry["offset"] = e.Offset;
            entry["size"] = e.Size;
            entry["payloadFormat"] = ToString(e.PayloadFormat);
            entry["compression"] = ToString(e.Compression);
            entry["uncompressedSize"] = e.UncompressedSize;
            entry["contentHash64"] = e.ContentHash64;
            manifest["entries"].push_back(std::move(entry));
        }

        const std::filesystem::path manifestPath = outputDirectory / "AssetBundleManifest.json";
        const std::string manifestText = manifest.dump(4);
        const std::vector<uint8_t> manifestBytes(manifestText.begin(), manifestText.end());
        auto writeManifestResult = AtomicWriteFile(manifestPath, manifestBytes);
        if (writeManifestResult.IsFailure())
        {
            return writeManifestResult;
        }

        LOG_CORE_INFO("AssetBundleBuilder: built {} entries ({} bytes) to '{}'",
            entries.size(), data.size(), outputDirectory.string());

        return Result<void>();
    }
}
