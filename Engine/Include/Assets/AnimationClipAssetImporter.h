#pragma once

#include "Assets/AssetImporter.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetHotReloadManager.h"
#include "Assets/AnimationClipAsset.h"

#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

namespace Life::Assets
{
    template<>
    struct AssetImporter<AnimationClipAsset>
    {
        using AssetT = AnimationClipAsset;
        using Ptr = AnimationClipAsset::Ptr;
        using Settings = AnimationClipAsset::Settings;

        static constexpr AssetType Type = AssetType::AnimationClip;
        static constexpr uint32_t Version = 1u;

        static nlohmann::json SettingsToJson(const Settings& s)
        {
            nlohmann::json j = nlohmann::json::object();
            j["validateStrictly"] = s.ValidateStrictly;
            return j;
        }

        static Ptr Load(const std::string& key, AssetDatabase& db)
        {
            Settings settings;
            const auto recordResult = db.ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
            if (recordResult.IsSuccess())
                db.SetDependencies(recordResult.GetValue().Guid, {});

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return AnimationClipAsset::LoadBlocking(key, settings);
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
            return AnimationClipAsset::LoadAsync(key, settings);
        }
    };
}
