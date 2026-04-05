#pragma once

#include "Assets/Asset.h"
#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace Life::Assets
{
    std::shared_ptr<Life::Asset> ResolveAssetByGuid(const std::string& guid);
}

namespace Life
{
    // -----------------------------------------------------------------------------
    // AssetHandle<T>
    // Lightweight weak reference to an asset by GUID.
    //
    // - Serialized as a GUID string in JSON.
    // - Resolves to a shared_ptr<T> via the AssetManager service.
    // - Caches the resolved pointer for fast repeated access.
    // -----------------------------------------------------------------------------
    template<typename T>
    class AssetHandle
    {
    public:
        AssetHandle() = default;

        explicit AssetHandle(std::string guid)
            : m_Guid(std::move(guid))
        {
        }

        explicit AssetHandle(const std::shared_ptr<T>& asset)
        {
            if (asset)
            {
                m_Guid = asset->GetGuid();
                m_Cached = asset;
            }
        }

        const std::string& GetGuid() const { return m_Guid; }
        bool HasGuid() const { return !m_Guid.empty(); }

        void SetGuid(const std::string& guid)
        {
            if (m_Guid != guid)
            {
                m_Guid = guid;
                m_Cached.reset();
            }
        }

        void Reset()
        {
            m_Guid.clear();
            m_Cached.reset();
        }

        std::shared_ptr<T> Resolve() const
        {
            if (m_Guid.empty()) return nullptr;

            if (auto locked = m_Cached.lock())
            {
                return locked;
            }

            // Resolve through global service registry
            auto asset = std::dynamic_pointer_cast<T>(Assets::ResolveAssetByGuid(m_Guid));
            if (asset)
            {
                m_Cached = asset;
            }
            return asset;
        }

        std::shared_ptr<T> Get() const { return Resolve(); }

        explicit operator bool() const
        {
            if (m_Guid.empty()) return false;
            if (auto locked = m_Cached.lock()) return true;
            return false;
        }

        T* operator->() const
        {
            auto resolved = Resolve();
            return resolved ? resolved.get() : nullptr;
        }

        bool operator==(const AssetHandle& other) const { return m_Guid == other.m_Guid; }
        bool operator!=(const AssetHandle& other) const { return m_Guid != other.m_Guid; }

        // JSON serialization
        friend void to_json(nlohmann::json& j, const AssetHandle<T>& handle)
        {
            j = handle.m_Guid;
        }

        friend void from_json(const nlohmann::json& j, AssetHandle<T>& handle)
        {
            if (j.is_string())
            {
                handle.SetGuid(j.get<std::string>());
            }
            else
            {
                handle.Reset();
            }
        }

    private:
        std::string m_Guid;
        mutable std::weak_ptr<T> m_Cached;
    };
}
