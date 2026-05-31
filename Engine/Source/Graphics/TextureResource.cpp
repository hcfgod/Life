#include "Core/LifePCH.h"
#include "Graphics/TextureResource.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsTypesInternal.h"
#include "Graphics/VertexLayout.h"

#include <nvrhi/nvrhi.h>

#include <atomic>

namespace Life
{
    namespace
    {
        std::atomic<uint64_t> s_NextTextureRuntimeId{ 1u };
    }

    struct TextureResource::Impl
    {
        nvrhi::TextureHandle Handle;
    };

    TextureResource::~TextureResource() = default;

    TextureResource::TextureResource(TextureResource&& other) noexcept
        : m_Impl(std::move(other.m_Impl))
        , m_Description(std::move(other.m_Description))
        , m_RuntimeId(other.m_RuntimeId)
    {
        other.m_RuntimeId = 0;
    }

    TextureResource& TextureResource::operator=(TextureResource&& other) noexcept
    {
        if (this != &other)
        {
            m_Impl = std::move(other.m_Impl);
            m_Description = std::move(other.m_Description);
            m_RuntimeId = other.m_RuntimeId;
            other.m_RuntimeId = 0;
        }
        return *this;
    }

    bool TextureResource::IsValid() const noexcept
    {
        return m_Impl && m_Impl->Handle;
    }

    nvrhi::ITexture* TextureResource::GetNativeHandle() const
    {
        return m_Impl ? m_Impl->Handle.Get() : nullptr;
    }

    Scope<TextureResource> TextureResource::Create2D(GraphicsDevice& device, const TextureDescription& desc,
                                                      const void* initialData)
    {
        nvrhi::IDevice* nvrhiDevice = device.GetNvrhiDevice();
        if (!nvrhiDevice)
            return nullptr;

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = desc.Width;
        textureDesc.height = desc.Height;
        textureDesc.mipLevels = desc.MipLevels;
        textureDesc.sampleCount = desc.SampleCount;
        textureDesc.format = Internal::ToNvrhiFormat(desc.Format);
        textureDesc.debugName = desc.DebugName.c_str();
        textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
        textureDesc.isRenderTarget = desc.IsRenderTarget;
        textureDesc.isUAV = desc.IsUAV;
        textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        textureDesc.keepInitialState = true;

        if (desc.IsRenderTarget)
        {
            textureDesc.initialState = nvrhi::ResourceStates::RenderTarget;
        }

        if (desc.IsDepthStencil)
        {
            textureDesc.initialState = nvrhi::ResourceStates::DepthWrite;
            textureDesc.isRenderTarget = true;
        }

        nvrhi::TextureHandle handle = nvrhiDevice->createTexture(textureDesc);
        if (!handle)
            return nullptr;

        if (initialData)
        {
            nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();
            commandList->open();
            size_t rowPitch = static_cast<size_t>(desc.Width) * GetFormatSizeBytes(desc.Format);
            commandList->writeTexture(handle, 0, 0, initialData, rowPitch);
            commandList->close();
            nvrhiDevice->executeCommandList(commandList);
        }

        Scope<TextureResource> texture(new TextureResource());
        texture->m_Impl = CreateScope<Impl>();
        texture->m_Impl->Handle = handle;
        texture->m_Description = desc;
        texture->m_RuntimeId = s_NextTextureRuntimeId.fetch_add(1u, std::memory_order_relaxed);
        return texture;
    }
}
