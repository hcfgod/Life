#include "Assets/PrefabAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"
#include "Scene/SceneSerializer.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

namespace Life::Assets
{
    PrefabAsset::PrefabAsset(std::string key, std::string guid, Scope<Scene> scene, Settings settings)
        : Asset(std::move(key), std::move(guid))
        , m_Scene(std::move(scene))
        , m_Settings(settings)
    {
    }

    std::future<PrefabAsset::Ptr> PrefabAsset::LoadAsync(const std::string& key)
    {
        return LoadAsync(key, Settings{});
    }

    std::future<PrefabAsset::Ptr> PrefabAsset::LoadAsync(const std::string& key, const Settings& settings)
    {
        return std::async(std::launch::async, [key, settings]() -> Ptr {
            const auto resolvedResult = ResolveAssetKeyToPath(key);
            if (resolvedResult.IsFailure())
            {
                LOG_CORE_ERROR("PrefabAsset::LoadAsync: failed to resolve key '{}': {}", key, resolvedResult.GetError().GetErrorMessage());
                return nullptr;
            }

            const std::filesystem::path resolvedPath = resolvedResult.GetValue();
            const auto guidResult = LoadOrCreateGuid(resolvedPath.string(), {{"key", key}, {"type", "Prefab"}});
            if (guidResult.IsFailure())
            {
                LOG_CORE_ERROR("PrefabAsset::LoadAsync: meta GUID failed for '{}': {}", resolvedPath.string(), guidResult.GetError().GetErrorMessage());
                return nullptr;
            }

            auto* assetManager = GetServices().TryGet<AssetManager>();
            auto sceneResult = SceneSerializer::Load(resolvedPath, assetManager);
            if (sceneResult.IsFailure())
            {
                LOG_CORE_ERROR("PrefabAsset::LoadAsync: failed to load '{}': {}", resolvedPath.string(), sceneResult.GetError().GetErrorMessage());
                return nullptr;
            }

            return Ref<PrefabAsset>(new PrefabAsset(key, guidResult.GetValue(), std::move(sceneResult.GetValue()), settings));
        });
    }

    PrefabAsset::Ptr PrefabAsset::LoadBlocking(const std::string& key)
    {
        return LoadBlocking(key, Settings{});
    }

    PrefabAsset::Ptr PrefabAsset::LoadBlocking(const std::string& key, const Settings& settings)
    {
        auto future = LoadAsync(key, settings);
        future.wait();
        return future.get();
    }

    bool PrefabAsset::Reload()
    {
        Ptr reloaded = LoadBlocking(GetKey(), m_Settings);
        if (!reloaded || reloaded->GetGuid() != GetGuid())
            return false;

        m_Scene = std::move(reloaded->m_Scene);
        return true;
    }
}
