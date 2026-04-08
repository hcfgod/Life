#pragma once

#include "Assets/Asset.h"
#include "Assets/AssetHandle.h"
#include "Assets/ShaderAsset.h"
#include "Assets/TextureAsset.h"

#include <glm/glm.hpp>

#include <future>
#include <memory>
#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // MaterialAsset
    // Unity-style material referencing shader + textures via GUID handles.
    // Stored as JSON, supports hot reload via dependency tracking.
    // -----------------------------------------------------------------------------
    class MaterialAsset final : public Life::Asset
    {
    public:
        using Ptr = Ref<MaterialAsset>;

        struct Settings
        {
            // reserved for importer settings
        };

        static std::future<Ptr> LoadAsync(const std::string& key, Settings settings = {});
        static Ptr LoadBlocking(const std::string& key, Settings settings = {});

        bool Reload() override;

        const AssetHandle<ShaderAsset>& GetShaderHandle() const { return m_Shader; }
        const AssetHandle<TextureAsset>& GetMainTextureHandle() const { return m_MainTexture; }
        const AssetHandle<TextureAsset>& GetNormalTextureHandle() const { return m_NormalTexture; }

        bool HasMainTextureSubRect() const { return m_HasMainTextureSubRect; }
        const glm::vec2& GetMainTextureUvMin() const { return m_MainTextureUvMin; }
        const glm::vec2& GetMainTextureUvMax() const { return m_MainTextureUvMax; }

        float GetNormalStrength() const { return m_NormalStrength; }
        float GetRoughness() const { return m_Roughness; }
        float GetSpecularIntensity() const { return m_SpecularIntensity; }

    private:
        friend class MaterialAssetFactory;

        MaterialAsset(std::string key, std::string guid, Settings settings)
            : Asset(std::move(key), std::move(guid))
            , m_Settings(std::move(settings))
        {
        }

        bool LoadFromJsonFile();

        Settings m_Settings{};

        AssetHandle<ShaderAsset> m_Shader;
        AssetHandle<TextureAsset> m_MainTexture;
        AssetHandle<TextureAsset> m_NormalTexture;

        bool m_HasMainTextureSubRect = false;
        glm::vec2 m_MainTextureUvMin = glm::vec2(0.0f, 0.0f);
        glm::vec2 m_MainTextureUvMax = glm::vec2(1.0f, 1.0f);

        float m_NormalStrength = 1.0f;
        float m_Roughness = 0.5f;
        float m_SpecularIntensity = 0.5f;

        std::string m_ResolvedPath;
    };
}
