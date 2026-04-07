#include "Core/LifePCH.h"
#include "Graphics/Renderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsTypesInternal.h"
#include "Graphics/TextureResource.h"
#include "Core/Log.h"

#include <nvrhi/nvrhi.h>

#include <cstdio>
#include <unordered_map>

namespace Life
{
    namespace
    {
        bool UsesLinearFiltering(TextureFilterMode mode) noexcept
        {
            return mode == TextureFilterMode::Linear || mode == TextureFilterMode::LinearMipmapLinear;
        }

        bool UsesMipFiltering(TextureFilterMode mode) noexcept
        {
            return mode == TextureFilterMode::NearestMipmapNearest || mode == TextureFilterMode::LinearMipmapLinear;
        }

        nvrhi::SamplerAddressMode ToNvrhiSamplerAddressMode(TextureWrapMode mode) noexcept
        {
            switch (mode)
            {
            case TextureWrapMode::ClampToEdge:
                return nvrhi::SamplerAddressMode::ClampToEdge;
            case TextureWrapMode::MirroredRepeat:
                return nvrhi::SamplerAddressMode::MirroredRepeat;
            case TextureWrapMode::Repeat:
            default:
                return nvrhi::SamplerAddressMode::Repeat;
            }
        }

        nvrhi::SamplerDesc ToNvrhiSamplerDesc(const TextureSamplerDescription& samplerDescription) noexcept
        {
            nvrhi::SamplerDesc samplerDesc;
            samplerDesc.setMinFilter(UsesLinearFiltering(samplerDescription.MinFilter));
            samplerDesc.setMagFilter(UsesLinearFiltering(samplerDescription.MagFilter));
            samplerDesc.setMipFilter(UsesMipFiltering(samplerDescription.MinFilter));
            samplerDesc.setAddressU(ToNvrhiSamplerAddressMode(samplerDescription.WrapU));
            samplerDesc.setAddressV(ToNvrhiSamplerAddressMode(samplerDescription.WrapV));
            samplerDesc.setAddressW(ToNvrhiSamplerAddressMode(samplerDescription.WrapW));
            return samplerDesc;
        }

        struct TextureSamplerDescriptionHasher
        {
            size_t operator()(const TextureSamplerDescription& samplerDescription) const noexcept
            {
                size_t hash = std::hash<uint8_t>{}(static_cast<uint8_t>(samplerDescription.MinFilter));
                hash ^= std::hash<uint8_t>{}(static_cast<uint8_t>(samplerDescription.MagFilter)) << 1;
                hash ^= std::hash<uint8_t>{}(static_cast<uint8_t>(samplerDescription.WrapU)) << 2;
                hash ^= std::hash<uint8_t>{}(static_cast<uint8_t>(samplerDescription.WrapV)) << 3;
                hash ^= std::hash<uint8_t>{}(static_cast<uint8_t>(samplerDescription.WrapW)) << 4;
                return hash;
            }
        };

        struct TextureBindingCacheKey
        {
            nvrhi::IBindingLayout* Layout = nullptr;
            nvrhi::ITexture* Texture = nullptr;
            TextureSamplerDescription SamplerDescription{};

            bool operator==(const TextureBindingCacheKey& other) const noexcept
            {
                return Layout == other.Layout &&
                       Texture == other.Texture &&
                       SamplerDescription == other.SamplerDescription;
            }
        };

        struct TextureBindingCacheKeyHasher
        {
            size_t operator()(const TextureBindingCacheKey& key) const noexcept
            {
                const size_t layoutHash = std::hash<nvrhi::IBindingLayout*>{}(key.Layout);
                const size_t textureHash = std::hash<nvrhi::ITexture*>{}(key.Texture);
                const size_t samplerHash = TextureSamplerDescriptionHasher{}(key.SamplerDescription);
                return layoutHash ^ (textureHash << 1) ^ (samplerHash << 2);
            }
        };

        void ReportRendererTeardownException(const std::exception& exception) noexcept
        {
            std::fprintf(stderr, "Renderer teardown suppressed an exception: %s\n", exception.what());
        }

        void ReportRendererTeardownException() noexcept
        {
            std::fprintf(stderr, "Renderer teardown suppressed a non-standard exception.\n");
        }
    }

    struct Renderer::Impl
    {
        nvrhi::FramebufferHandle CurrentFramebuffer;
        TextureResource* ActiveColorTarget = nullptr;
        nvrhi::ITexture* CachedColorTarget = nullptr;
        nvrhi::IDevice* CachedDevice = nullptr;
        std::unordered_map<TextureSamplerDescription, nvrhi::SamplerHandle, TextureSamplerDescriptionHasher> TextureSamplers;
        std::unordered_map<TextureBindingCacheKey, nvrhi::BindingSetHandle, TextureBindingCacheKeyHasher> TextureBindingSets;
        uint32_t CachedFramebufferWidth = 0;
        uint32_t CachedFramebufferHeight = 0;
        nvrhi::Viewport PendingViewport = nvrhi::Viewport();
        nvrhi::Rect PendingScissor = nvrhi::Rect();
        bool HasPendingViewport = false;
        bool HasPendingScissor = false;
    };

    Renderer::Renderer(GraphicsDevice& graphicsDevice)
        : m_GraphicsDevice(graphicsDevice)
        , m_ShaderLibrary(graphicsDevice)
        , m_Impl(CreateScope<Impl>())
    {
        LOG_CORE_INFO("Renderer initialized.");
    }

    Renderer::~Renderer() noexcept
    {
        try
        {
            m_ShaderLibrary.Clear();
            if (m_Impl)
            {
                m_Impl->CurrentFramebuffer = nullptr;
                m_Impl->ActiveColorTarget = nullptr;
                m_Impl->CachedColorTarget = nullptr;
                m_Impl->CachedDevice = nullptr;
                m_Impl->TextureSamplers.clear();
                m_Impl->TextureBindingSets.clear();
                m_Impl->CachedFramebufferWidth = 0;
                m_Impl->CachedFramebufferHeight = 0;
            }
            LOG_CORE_INFO("Renderer destroyed.");
        }
        catch (const std::exception& exception)
        {
            ReportRendererTeardownException(exception);
        }
        catch (...)
        {
            ReportRendererTeardownException();
        }
    }

    void Renderer::BeginScene(const glm::mat4& viewProjection)
    {
        m_SceneData.ViewProjectionMatrix = viewProjection;
        m_SceneData.ViewMatrix = glm::mat4(1.0f);
        m_SceneData.ProjectionMatrix = glm::mat4(1.0f);
        m_InScene = true;
        ResetStats();
    }

    void Renderer::BeginScene(const glm::mat4& view, const glm::mat4& projection)
    {
        m_SceneData.ViewMatrix = view;
        m_SceneData.ProjectionMatrix = projection;
        m_SceneData.ViewProjectionMatrix = projection * view;
        m_InScene = true;
        ResetStats();
    }

    void Renderer::EndScene()
    {
        m_InScene = false;
    }

    void Renderer::Clear(float r, float g, float b, float a)
    {
        nvrhi::ITexture* colorTarget = m_Impl->ActiveColorTarget != nullptr
            ? m_Impl->ActiveColorTarget->GetNativeHandle()
            : m_GraphicsDevice.GetCurrentBackBuffer();
        nvrhi::ICommandList* commandList = m_GraphicsDevice.GetCurrentCommandList();

        if (!colorTarget || !commandList)
            return;

        nvrhi::Color clearColor(r, g, b, a);
        commandList->clearTextureFloat(colorTarget, nvrhi::AllSubresources, clearColor);
    }

    void Renderer::SetViewport(float x, float y, float width, float height)
    {
        m_Impl->PendingViewport = nvrhi::Viewport(x, x + width, y, y + height, 0.0f, 1.0f);
        m_Impl->HasPendingViewport = true;
    }

    void Renderer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        m_Impl->PendingScissor = nvrhi::Rect(
            x,
            static_cast<int32_t>(x + width),
            y,
            static_cast<int32_t>(y + height));
        m_Impl->HasPendingScissor = true;
    }

    void Renderer::Submit(GraphicsPipeline& pipeline,
                          GraphicsBuffer& vertexBuffer,
                          uint32_t vertexCount,
                          const TextureResource* texture)
    {
        nvrhi::ICommandList* commandList = m_GraphicsDevice.GetCurrentCommandList();
        if (!commandList)
            return;

        if (!pipeline.IsValid() || !vertexBuffer.IsValid())
        {
            LOG_CORE_WARN("Renderer::Submit: Invalid pipeline or vertex buffer.");
            return;
        }

        nvrhi::GraphicsState state;
        state.pipeline = pipeline.GetNativePipelineHandle();

        nvrhi::VertexBufferBinding vbBinding;
        vbBinding.buffer = vertexBuffer.GetNativeHandle();
        vbBinding.slot = 0;
        vbBinding.offset = 0;
        state.addVertexBuffer(vbBinding);

        EnsureFramebuffer();
        state.framebuffer = m_Impl->CurrentFramebuffer;

        if (m_Impl->HasPendingViewport)
        {
            state.viewport.addViewport(m_Impl->PendingViewport);
            if (m_Impl->HasPendingScissor)
                state.viewport.addScissorRect(m_Impl->PendingScissor);
            else
                state.viewport.addScissorRect(nvrhi::Rect(m_Impl->PendingViewport));
        }
        else
        {
            const FramebufferExtent framebufferExtent = GetFramebufferExtent();
            state.viewport.addViewportAndScissorRect(nvrhi::Viewport(
                static_cast<float>(framebufferExtent.Width),
                static_cast<float>(framebufferExtent.Height)));
        }

        if (pipeline.GetDescription().UseTextureBinding)
        {
            if (texture == nullptr || !texture->IsValid())
            {
                LOG_CORE_WARN("Renderer::Submit: Textured pipeline '{}' received no valid texture.",
                              pipeline.GetDescription().DebugName);
                return;
            }

            nvrhi::IDevice* nvrhiDevice = m_GraphicsDevice.GetNvrhiDevice();
            if (!nvrhiDevice)
                return;

            nvrhi::IBindingLayout* bindingLayout = pipeline.GetNativeBindingLayoutHandle();
            if (!bindingLayout)
            {
                LOG_CORE_ERROR("Renderer::Submit: Textured pipeline '{}' has no binding layout.",
                               pipeline.GetDescription().DebugName);
                return;
            }

            nvrhi::ITexture* nativeTexture = texture->GetNativeHandle();
            if (!nativeTexture)
            {
                LOG_CORE_WARN("Renderer::Submit: Textured pipeline '{}' received a texture with no native handle.",
                              pipeline.GetDescription().DebugName);
                return;
            }

            const TextureSamplerDescription& samplerDescription = texture->GetSamplerDescription();
            auto samplerIt = m_Impl->TextureSamplers.find(samplerDescription);
            if (samplerIt == m_Impl->TextureSamplers.end())
            {
                nvrhi::SamplerHandle sampler = nvrhiDevice->createSampler(ToNvrhiSamplerDesc(samplerDescription));
                if (!sampler)
                {
                    LOG_CORE_ERROR("Renderer::Submit: Failed to create texture sampler for pipeline '{}'.",
                                   pipeline.GetDescription().DebugName);
                    return;
                }

                samplerIt = m_Impl->TextureSamplers.emplace(samplerDescription, std::move(sampler)).first;
            }

            const TextureBindingCacheKey cacheKey{ bindingLayout, nativeTexture, samplerDescription };
            auto bindingSetIt = m_Impl->TextureBindingSets.find(cacheKey);
            if (bindingSetIt == m_Impl->TextureBindingSets.end())
            {
                nvrhi::BindingSetDesc bindingSetDesc;
                bindingSetDesc.trackLiveness = true;
                bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, nativeTexture));
                bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, samplerIt->second.Get()));

                nvrhi::BindingSetHandle bindingSet = nvrhiDevice->createBindingSet(bindingSetDesc, bindingLayout);
                if (!bindingSet)
                {
                    LOG_CORE_ERROR("Renderer::Submit: Failed to create texture binding set for pipeline '{}'.",
                                   pipeline.GetDescription().DebugName);
                    return;
                }

                bindingSetIt = m_Impl->TextureBindingSets.emplace(cacheKey, std::move(bindingSet)).first;
            }

            state.addBindingSet(bindingSetIt->second.Get());
        }

        commandList->setGraphicsState(state);

        nvrhi::DrawArguments drawArgs;
        drawArgs.vertexCount = vertexCount;
        commandList->draw(drawArgs);

        m_Stats.DrawCalls++;
        m_Stats.VerticesSubmitted += vertexCount;
    }

    void Renderer::SubmitIndexed(GraphicsPipeline& pipeline,
                                  GraphicsBuffer& vertexBuffer,
                                  GraphicsBuffer& indexBuffer,
                                  const IndexedDrawParameters& drawParameters)
    {
        nvrhi::ICommandList* commandList = m_GraphicsDevice.GetCurrentCommandList();
        if (!commandList)
            return;

        if (!pipeline.IsValid() || !vertexBuffer.IsValid() || !indexBuffer.IsValid())
        {
            LOG_CORE_WARN("Renderer::SubmitIndexed: Invalid pipeline, vertex buffer, or index buffer.");
            return;
        }

        nvrhi::GraphicsState state;
        state.pipeline = pipeline.GetNativePipelineHandle();

        nvrhi::VertexBufferBinding vbBinding;
        vbBinding.buffer = vertexBuffer.GetNativeHandle();
        vbBinding.slot = 0;
        vbBinding.offset = 0;
        state.addVertexBuffer(vbBinding);

        nvrhi::IndexBufferBinding ibBinding;
        ibBinding.buffer = indexBuffer.GetNativeHandle();
        ibBinding.format = (indexBuffer.GetStride() == 4) ? nvrhi::Format::R32_UINT : nvrhi::Format::R16_UINT;
        ibBinding.offset = 0;
        state.indexBuffer = ibBinding;

        EnsureFramebuffer();
        state.framebuffer = m_Impl->CurrentFramebuffer;

        if (m_Impl->HasPendingViewport)
        {
            state.viewport.addViewport(m_Impl->PendingViewport);
            if (m_Impl->HasPendingScissor)
                state.viewport.addScissorRect(m_Impl->PendingScissor);
            else
                state.viewport.addScissorRect(nvrhi::Rect(m_Impl->PendingViewport));
        }
        else
        {
            const FramebufferExtent framebufferExtent = GetFramebufferExtent();
            state.viewport.addViewportAndScissorRect(nvrhi::Viewport(
                static_cast<float>(framebufferExtent.Width),
                static_cast<float>(framebufferExtent.Height)));
        }

        commandList->setGraphicsState(state);

        nvrhi::DrawArguments drawArgs;
        drawArgs.vertexCount = drawParameters.IndexCount;
        drawArgs.startIndexLocation = drawParameters.IndexOffset;
        commandList->drawIndexed(drawArgs);

        m_Stats.DrawCalls++;
        m_Stats.IndicesSubmitted += drawParameters.IndexCount;
    }

    void Renderer::SetRenderTarget(TextureResource* colorTarget)
    {
        if (m_Impl->ActiveColorTarget == colorTarget)
            return;

        m_Impl->ActiveColorTarget = colorTarget;
        m_Impl->CurrentFramebuffer = nullptr;
        m_Impl->CachedColorTarget = nullptr;
        m_Impl->CachedDevice = nullptr;
        m_Impl->CachedFramebufferWidth = 0;
        m_Impl->CachedFramebufferHeight = 0;
    }

    TextureResource* Renderer::GetRenderTarget() const noexcept
    {
        return m_Impl->ActiveColorTarget;
    }

    FramebufferExtent Renderer::GetFramebufferExtent() const
    {
        if (m_Impl->ActiveColorTarget != nullptr)
        {
            return FramebufferExtent
            {
                m_Impl->ActiveColorTarget->GetWidth(),
                m_Impl->ActiveColorTarget->GetHeight()
            };
        }

        return FramebufferExtent
        {
            m_GraphicsDevice.GetBackBufferWidth(),
            m_GraphicsDevice.GetBackBufferHeight()
        };
    }

    Scope<GraphicsPipeline> Renderer::CreatePipeline(const GraphicsPipelineDescription& desc)
    {
        EnsureFramebuffer();
        return GraphicsPipeline::Create(m_GraphicsDevice, desc, m_Impl->CurrentFramebuffer.Get());
    }

    nvrhi::IFramebuffer* Renderer::GetCurrentFramebuffer()
    {
        EnsureFramebuffer();
        return m_Impl->CurrentFramebuffer.Get();
    }

    void Renderer::ResetStats() noexcept
    {
        m_Stats = {};
    }

    void Renderer::EnsureFramebuffer()
    {
        nvrhi::IDevice* nvrhiDevice = m_GraphicsDevice.GetNvrhiDevice();
        nvrhi::ITexture* colorTarget = m_Impl->ActiveColorTarget != nullptr
            ? m_Impl->ActiveColorTarget->GetNativeHandle()
            : m_GraphicsDevice.GetCurrentBackBuffer();
        const FramebufferExtent framebufferExtent = GetFramebufferExtent();

        if (!nvrhiDevice || !colorTarget)
        {
            m_Impl->CurrentFramebuffer = nullptr;
            m_Impl->CachedColorTarget = nullptr;
            m_Impl->CachedDevice = nullptr;
            m_Impl->CachedFramebufferWidth = 0;
            m_Impl->CachedFramebufferHeight = 0;
            return;
        }

        if (m_Impl->CurrentFramebuffer
            && m_Impl->CachedColorTarget == colorTarget
            && m_Impl->CachedDevice == nvrhiDevice
            && m_Impl->CachedFramebufferWidth == framebufferExtent.Width
            && m_Impl->CachedFramebufferHeight == framebufferExtent.Height)
        {
            return;
        }

        nvrhi::FramebufferDesc fbDesc;
        fbDesc.addColorAttachment(colorTarget);

        m_Impl->CurrentFramebuffer = nvrhiDevice->createFramebuffer(fbDesc);
        if (!m_Impl->CurrentFramebuffer)
        {
            m_Impl->CachedColorTarget = nullptr;
            m_Impl->CachedDevice = nullptr;
            m_Impl->CachedFramebufferWidth = 0;
            m_Impl->CachedFramebufferHeight = 0;
            return;
        }

        m_Impl->CachedColorTarget = colorTarget;
        m_Impl->CachedDevice = nvrhiDevice;
        m_Impl->CachedFramebufferWidth = framebufferExtent.Width;
        m_Impl->CachedFramebufferHeight = framebufferExtent.Height;
    }
}
