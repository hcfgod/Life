#pragma once

#include "Assets/AssetTypes.h"
#include "Core/Error.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetBundleCompression / PayloadFormat
    // -----------------------------------------------------------------------------
    enum class AssetBundleCompression : uint8_t
    {
        None = 0,
        Zstd = 1
    };

    enum class AssetBundlePayloadFormat : uint8_t
    {
        Raw = 0,
        CookedTexture2D = 1,
        CookedShaderStages = 2
    };

    // -----------------------------------------------------------------------------
    // AssetBundle
    // Runtime asset bundle reader. Loads a manifest + data file pair and provides
    // thread-safe byte reads by key or GUID.
    //
    // Host-owned service — create via ApplicationHost, register in ServiceRegistry.
    // -----------------------------------------------------------------------------
    class AssetBundle final
    {
    public:
        struct Entry
        {
            std::string Guid;
            std::string Key;
            AssetType Type = AssetType::Unknown;
            uint64_t Offset = 0;
            uint64_t Size = 0;
            AssetBundlePayloadFormat PayloadFormat = AssetBundlePayloadFormat::Raw;
            AssetBundleCompression Compression = AssetBundleCompression::None;
            uint64_t UncompressedSize = 0;
            uint64_t ContentHash64 = 0;
        };

        AssetBundle() = default;
        ~AssetBundle() = default;

        AssetBundle(const AssetBundle&) = delete;
        AssetBundle& operator=(const AssetBundle&) = delete;

        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool IsEnabled() const { return m_Enabled; }
        bool IsLoaded() const { return m_Loaded; }

        void Unload();

        Result<void> LoadFromManifestFile(const std::filesystem::path& manifestPath);
        Result<void> LoadFromProjectBuildOutput();
        Result<void> LoadFromDirectory(const std::filesystem::path& directory);
        Result<void> LoadFromExecutableDirectory();

        std::optional<Entry> FindEntryByKey(const std::string& key) const;
        std::optional<Entry> FindEntryByGuid(const std::string& guid) const;
        std::optional<std::string> FindGuidByKey(const std::string& key) const;
        std::optional<std::string> FindKeyByGuid(const std::string& guid) const;

        Result<std::vector<uint8_t>> ReadAllBytesByKey(const std::string& key);
        Result<std::vector<uint8_t>> ReadAllBytesByGuid(const std::string& guid);
        Result<std::string> ReadAllTextByKey(const std::string& key);
        Result<std::string> ReadAllTextByGuid(const std::string& guid);

    private:
        Result<std::vector<uint8_t>> ReadAllBytesByEntryLocked(const Entry& entry);

        bool m_Enabled = false;
        bool m_Loaded = false;
        std::filesystem::path m_ManifestPath;
        std::filesystem::path m_DataPath;

        std::unordered_map<std::string, Entry> m_ByKey;
        std::unordered_map<std::string, std::string> m_KeyByGuid;

        std::mutex m_ReadMutex;
        std::ifstream m_DataStream;
    };
}
