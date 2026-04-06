#pragma once

#include "Assets/Asset.h"
#include "Assets/TextureSpecificationJson.h"
#include "Graphics/TextureResource.h"
#include "Core/Memory.h"

#include <future>
#include <memory>
#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // TextureAsset
    // Wraps a GPU TextureResource loaded from the asset pipeline.
    // CPU decode on background thread, GPU upload via GraphicsDevice.
    // -----------------------------------------------------------------------------
    class TextureAsset final : public Life::Asset
    {
    public:
        using Ptr = std::shared_ptr<TextureAsset>;

        static std::future<Ptr> LoadAsync(const std::string& assetPath, const TextureSpecification& specification = {});
        static Ptr LoadBlocking(const std::string& assetPath, const TextureSpecification& specification = {});

        TextureResource* GetTexture() const { return m_Texture.get(); }
        const TextureSpecification& GetSpecification() const { return m_Specification; }
        void SetSpecification(const TextureSpecification& spec) { ApplySpecification(spec); }
        void ApplySpecification(const TextureSpecification& spec);
        void SetSamplerDescription(const TextureSamplerDescription& samplerDescription)
        {
            if (m_Texture)
                m_Texture->SetSamplerDescription(samplerDescription);
        }

        void SetFilterModes(TextureFilterMode minFilter, TextureFilterMode magFilter)
        {
            if (m_Texture)
                m_Texture->SetFilterModes(minFilter, magFilter);
        }

        void SetWrapModes(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW = TextureWrapMode::Repeat)
        {
            if (m_Texture)
                m_Texture->SetWrapModes(wrapU, wrapV, wrapW);
        }

        bool Reload() override;

    private:
        friend class TextureAssetFactory;

        TextureAsset(std::string key, std::string guid, Scope<TextureResource> texture, TextureSpecification specification)
            : Asset(std::move(key), std::move(guid))
            , m_Texture(std::move(texture))
            , m_Specification(specification)
        {
        }

        Scope<TextureResource> m_Texture;
        TextureSpecification m_Specification{};
    };
}
