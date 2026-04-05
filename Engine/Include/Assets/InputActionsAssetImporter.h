#pragma once

#include "Assets/AssetImporter.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetHotReloadManager.h"
#include "Assets/InputActionsAssetResource.h"

#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

namespace Life::Assets
{
    template<>
    struct AssetImporter<InputActionsAssetResource>
    {
        using AssetT = InputActionsAssetResource;
        using Ptr = InputActionsAssetResource::Ptr;
        using Settings = InputActionsAssetResource::Settings;

        static constexpr AssetType Type = AssetType::InputActions;
        static constexpr uint32_t Version = 1u;

        static nlohmann::json SettingsToJson(const Settings&)
        {
            return nlohmann::json::object();
        }

        static Ptr Load(const std::string& key, AssetDatabase& db)
        {
            Settings settings;
            const auto recordResult = db.ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
            (void)recordResult;

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return InputActionsAssetResource::LoadBlocking(key, settings);
        }

        static std::future<Ptr> LoadAsync(const std::string& key, const Settings& settings = Settings{})
        {
            auto* db = GetServices().TryGet<AssetDatabase>();
            if (db)
            {
                const auto recordResult = db->ImportOrUpdate(key, Type, SettingsToJson(settings), Version);
                (void)recordResult;
            }

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return InputActionsAssetResource::LoadAsync(key, settings);
        }
    };
}
