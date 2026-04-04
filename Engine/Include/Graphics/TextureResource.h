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

    struct TextureDescription
    {
        std::string DebugName;
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t MipLevels = 1;
        TextureFormat Format = TextureFormat::RGBA8_UNORM;
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
        bool IsValid() const noexcept;

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
