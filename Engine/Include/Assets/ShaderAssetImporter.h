#pragma once

#include "Assets/AssetImporter.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetHotReloadManager.h"
#include "Assets/ShaderAsset.h"

#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

namespace Life::Assets
{
    template<>
    struct AssetImporter<ShaderAsset>
    {
        using AssetT = ShaderAsset;
        using Ptr = ShaderAsset::Ptr;
        using Settings = ShaderAsset::Settings;

        static constexpr AssetType Type = AssetType::Shader;
        static constexpr uint32_t Version = 1u;

        static nlohmann::json SettingsToJson(const Settings& s)
        {
            nlohmann::json j = nlohmann::json::object();
            if (!s.Name.empty())
                j["name"] = s.Name;
            return j;
        }

        static Ptr Load(const std::string& key, AssetDatabase& db)
        {
            Settings settings;
            const auto recordResult = db.ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
            if (recordResult.IsSuccess())
                db.SetDependencies(recordResult.GetValue().Guid, {});

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return ShaderAsset::LoadBlocking(key, settings);
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
            return ShaderAsset::LoadAsync(key, settings);
        }
    };
}
