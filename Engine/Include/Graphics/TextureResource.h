#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Memory.h"

#include <cstdint>
#include <string>

namespace nvrhi
{
    class ITexture;
}

namespace Life
{
    class GraphicsDevice;

    enum class TextureFilterMode : uint8_t
    {
        Nearest = 0,
        Linear = 1,
        NearestMipmapNearest = 2,
        LinearMipmapLinear = 3
    };

    enum class TextureWrapMode : uint8_t
    {
        Repeat = 0,
        ClampToEdge = 1,
        MirroredRepeat = 2
    };

    struct TextureSamplerDescription
    {
        TextureFilterMode MinFilter = TextureFilterMode::Linear;
        TextureFilterMode MagFilter = TextureFilterMode::Linear;
        TextureWrapMode WrapU = TextureWrapMode::Repeat;
        TextureWrapMode WrapV = TextureWrapMode::Repeat;
        TextureWrapMode WrapW = TextureWrapMode::Repeat;

        bool operator==(const TextureSamplerDescription&) const noexcept = default;
    };

    struct TextureDescription
    {
        std::string DebugName;
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t MipLevels = 1;
        TextureFormat Format = TextureFormat::RGBA8_UNORM;
        TextureSamplerDescription Sampler;
        bool IsRenderTarget = false;
        bool IsDepthStencil = false;
        bool IsUAV = false;
    };

    class TextureResource
    {
    public:
        ~TextureResource();

        TextureResource(const TextureResource&) = delete;
        TextureResource& operator=(const TextureResource&) = delete;
        TextureResource(TextureResource&&) noexcept;
        TextureResource& operator=(TextureResource&&) noexcept;

        const TextureDescription& GetDescription() const noexcept { return m_Description; }
        uint32_t GetWidth() const noexcept { return m_Description.Width; }
        uint32_t GetHeight() const noexcept { return m_Description.Height; }
        TextureFormat GetFormat() const noexcept { return m_Description.Format; }
        const TextureSamplerDescription& GetSamplerDescription() const noexcept { return m_Description.Sampler; }
        bool IsValid() const noexcept;

        void SetSamplerDescription(const TextureSamplerDescription& samplerDescription) noexcept
        {
            m_Description.Sampler = samplerDescription;
        }

        void SetFilterModes(TextureFilterMode minFilter, TextureFilterMode magFilter) noexcept
        {
            m_Description.Sampler.MinFilter = minFilter;
            m_Description.Sampler.MagFilter = magFilter;
        }

        void SetWrapModes(TextureWrapMode wrapU, TextureWrapMode wrapV, TextureWrapMode wrapW = TextureWrapMode::Repeat) noexcept
        {
            m_Description.Sampler.WrapU = wrapU;
            m_Description.Sampler.WrapV = wrapV;
            m_Description.Sampler.WrapW = wrapW;
        }

        nvrhi::ITexture* GetNativeHandle() const;

        static Scope<TextureResource> Create2D(GraphicsDevice& device, const TextureDescription& desc,
                                                const void* initialData = nullptr);

    private:
        friend class Renderer;

        TextureResource() = default;

        struct Impl;
        Scope<Impl> m_Impl;
        TextureDescription m_Description;
    };
}
