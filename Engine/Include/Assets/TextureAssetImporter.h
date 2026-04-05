#pragma once

#include "Assets/AssetImporter.h"
#include "Assets/AssetDatabase.h"
#include "Assets/AssetHotReloadManager.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/TextureAsset.h"
#include "Assets/TextureSpecificationJson.h"

#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

namespace Life::Assets
{
    template<>
    struct AssetImporter<TextureAsset>
    {
        using AssetT = TextureAsset;
        using Ptr = TextureAsset::Ptr;
        using Settings = TextureSpecification;

        static constexpr AssetType Type = AssetType::Texture2D;
        static constexpr uint32_t Version = 1u;

        static nlohmann::json SettingsToJson(const Settings& s)
        {
            nlohmann::json j = nlohmann::json::object();
            j["generateMipmaps"] = s.GenerateMipmaps;
            j["flipVerticallyOnLoad"] = s.FlipVerticallyOnLoad;
            j["minFilter"] = static_cast<int>(s.MinFilter);
            j["magFilter"] = static_cast<int>(s.MagFilter);
            j["wrapU"] = static_cast<int>(s.WrapU);
            j["wrapV"] = static_cast<int>(s.WrapV);
            return j;
        }

        static Ptr Load(const std::string& key, AssetDatabase& db)
        {
            Settings resolvedSettings;
            const auto recordResult = db.ImportOrUpdate(key, Type, nlohmann::json::object(), Version);
            if (recordResult.IsSuccess())
            {
                db.SetDependencies(recordResult.GetValue().Guid, {});
                resolvedSettings = TextureSpecificationFromImporterSettingsJson(recordResult.GetValue().ImporterSettings);
            }

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return TextureAsset::LoadBlocking(key, resolvedSettings);
        }

        static std::future<Ptr> LoadAsync(const std::string& key, const Settings& settings = Settings{})
        {
            const bool isDefaultSettings = IsDefaultTextureSpecification(settings);

            Settings resolvedSettings = settings;
            auto* db = GetServices().TryGet<AssetDatabase>();
            if (db)
            {
                const auto recordResult = db->ImportOrUpdate(
                    key,
                    Type,
                    isDefaultSettings ? nlohmann::json::object() : SettingsToJson(settings),
                    Version);
                if (recordResult.IsSuccess())
                {
                    db->SetDependencies(recordResult.GetValue().Guid, {});
                    if (isDefaultSettings)
                        resolvedSettings = TextureSpecificationFromImporterSettingsJson(recordResult.GetValue().ImporterSettings);
                }
            }

            AssetHotReloadManager::GetInstance().WatchKey(key);
            return TextureAsset::LoadAsync(key, resolvedSettings);
        }
    };
}
