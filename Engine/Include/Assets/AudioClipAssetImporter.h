#pragma once

#include "Assets/AssetImporter.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetHotReloadManager.h"
#include "Assets/AudioClipAsset.h"

#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

namespace Life::Assets
{
    template<>
    struct AssetImporter<AudioClipAsset>
    {
        using AssetT = AudioClipAsset;
        using Ptr = AudioClipAsset::Ptr;
        using Settings = AudioClipAsset::Settings;

        static constexpr AssetType Type = AssetType::AudioClip;
        static constexpr uint32_t Version = 1u;

        static nlohmann::json SettingsToJson(const Settings& s)
        {
            nlohmann::json j = nlohmann::json::object();
            j["targetSampleRateHz"] = s.TargetSampleRateHz;
            j["targetChannelCount"] = s.TargetChannelCount;
            return j;
        }

        static Ptr Load(const std::string& key, AssetDatabase& db)
        {
            Settings settings;
            const auto recordResult = db.ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
            if (recordResult.IsSuccess())
                db.SetDependencies(recordResult.GetValue().Guid, {});

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return AudioClipAsset::LoadBlocking(key, settings);
        }

        static std::future<Ptr> LoadAsync(const std::string& key, const Settings& settings = Settings{})
        {
            auto* db = GetServices().TryGet<AssetDatabase>();
            if (db)
            {
                const auto recordResult = db->ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
                if (recordResult.IsSuccess())
                    db->SetDependencies(recordResult.GetValue().Guid, {});
            }

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return AudioClipAsset::LoadAsync(key, settings);
        }
    };
}
