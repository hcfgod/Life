#pragma once

#include "Assets/AssetContext.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetImporter.h"
#include "Assets/PrefabAsset.h"

#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

namespace Life::Assets
{
    template<>
    struct AssetImporter<PrefabAsset>
    {
        using AssetT = PrefabAsset;
        using Ptr = PrefabAsset::Ptr;
        using Settings = PrefabAsset::Settings;

        static constexpr AssetType Type = AssetType::Prefab;
        static constexpr uint32_t Version = 1u;

        static nlohmann::json SettingsToJson(const Settings&)
        {
            return nlohmann::json::object();
        }

        static Ptr Load(const std::string& key, AssetDatabase& db)
        {
            Settings settings;
            const auto recordResult = db.ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
            if (recordResult.IsSuccess())
                db.SetDependencies(recordResult.GetValue().Guid, {});

            GetAssetHotReloadManager().WatchKey(key);
            return PrefabAsset::LoadBlocking(key, settings);
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

            GetAssetHotReloadManager().WatchKey(key);
            return PrefabAsset::LoadAsync(key, settings);
        }
    };
}
