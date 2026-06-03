#pragma once

#include "Assets/Asset.h"
#include "Core/Memory.h"
#include "Scene/Scene.h"

#include <future>
#include <string>

namespace Life::Assets
{
    class PrefabAsset final : public Life::Asset
    {
    public:
        using Ptr = Ref<PrefabAsset>;

        struct Settings
        {
        };

        static std::future<Ptr> LoadAsync(const std::string& key);
        static std::future<Ptr> LoadAsync(const std::string& key, const Settings& settings);
        static Ptr LoadBlocking(const std::string& key);
        static Ptr LoadBlocking(const std::string& key, const Settings& settings);

        const Scene* GetPrefabScene() const noexcept { return m_Scene.get(); }
        Scene* GetPrefabScene() noexcept { return m_Scene.get(); }

        bool Reload() override;

    private:
        PrefabAsset(std::string key, std::string guid, Scope<Scene> scene, Settings settings);

        Scope<Scene> m_Scene;
        Settings m_Settings{};
    };
}
