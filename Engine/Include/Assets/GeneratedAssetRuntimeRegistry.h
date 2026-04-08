#pragma once

#include "Assets/Asset.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // GeneratedAssetRuntimeRegistry
    // Registry for "virtual" generated assets that have no source file on disk.
    // These assets are created by code (e.g. procedural textures, default materials)
    // and registered with a factory callback.
    //
    // Singleton — stateless global utility.
    // -----------------------------------------------------------------------------
    class GeneratedAssetRuntimeRegistry final
    {
    public:
        using FactoryFn = std::function<Ref<Asset>()>;
        using ReloadFn = std::function<bool(const std::string& virtualKey)>;
        using LoadFn = std::function<Ref<Asset>(const std::string& virtualKey)>;

        static GeneratedAssetRuntimeRegistry& GetInstance()
        {
            static GeneratedAssetRuntimeRegistry s_Instance;
            return s_Instance;
        }

        void Register(const std::string& virtualKey, FactoryFn factory, ReloadFn reload = nullptr, LoadFn load = nullptr)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            Entry entry;
            entry.Factory = std::move(factory);
            entry.ReloadCallback = std::move(reload);
            entry.LoadCallback = std::move(load);
            m_Entries[virtualKey] = std::move(entry);
        }

        void Unregister(const std::string& virtualKey)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Entries.erase(virtualKey);
        }

        bool IsRegistered(const std::string& virtualKey) const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Entries.find(virtualKey) != m_Entries.end();
        }

        Ref<Asset> Create(const std::string& virtualKey)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_Entries.find(virtualKey);
            if (it == m_Entries.end() || !it->second.Factory)
                return nullptr;
            return it->second.Factory();
        }

        Ref<Asset> Load(const std::string& virtualKey)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_Entries.find(virtualKey);
            if (it == m_Entries.end())
                return nullptr;
            if (it->second.LoadCallback)
                return it->second.LoadCallback(virtualKey);
            if (it->second.Factory)
                return it->second.Factory();
            return nullptr;
        }

        bool Reload(const std::string& virtualKey)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_Entries.find(virtualKey);
            if (it == m_Entries.end() || !it->second.ReloadCallback)
                return false;
            return it->second.ReloadCallback(virtualKey);
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Entries.clear();
        }

    private:
        GeneratedAssetRuntimeRegistry() = default;

        struct Entry
        {
            FactoryFn Factory;
            ReloadFn ReloadCallback;
            LoadFn LoadCallback;
        };

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, Entry> m_Entries;
    };
}
