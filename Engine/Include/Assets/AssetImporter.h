#pragma once

#include "Assets/AssetDatabase.h"

#include <memory>
#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetImporter<TAsset>
    // Template trait specialized per asset type. Each specialization must provide:
    //
    //   static constexpr uint32_t Version;
    //   static std::shared_ptr<TAsset> Load(const std::string& key, AssetDatabase& db);
    //   static nlohmann::json DefaultImporterSettings();
    //
    // The AssetManager calls AssetImporter<T>::Load() when a cache miss occurs.
    // -----------------------------------------------------------------------------
    template<typename TAsset>
    struct AssetImporter
    {
        static constexpr uint32_t Version = 1;

        static std::shared_ptr<TAsset> Load(const std::string& key, AssetDatabase& db)
        {
            (void)key;
            (void)db;
            return nullptr;
        }

        static nlohmann::json DefaultImporterSettings()
        {
            return nlohmann::json::object();
        }
    };
}
