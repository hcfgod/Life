#include "Core/LifePCH.h"
#include "Graphics/Renderer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsTypesInternal.h"
#include "Core/Log.h"

#include <nvrhi/nvrhi.h>

namespace Life
{
    struct Renderer::Impl
    {
        nvrhi::FramebufferHandle CurrentFramebuffer;
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

    Renderer::~Renderer()
    {
        m_ShaderLibrary.Clear();
        if (m_Impl)
            m_Impl->CurrentFramebuffer = nullptr;
        LOG_CORE_INFO("Renderer destroyed.");
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
        nvrhi::ITexture* backBuffer = m_GraphicsDevice.GetCurrentBackBuffer();
        nvrhi::ICommandList* commandList = m_GraphicsDevice.GetCurrentCommandList();

        if (!backBuffer || !commandList)
            return;

        nvrhi::Color clearColor(r, g, b, a);
        commandList->clearTextureFloat(backBuffer, nvrhi::AllSubresources, clearColor);
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
                          uint32_t vertexCount)
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
            state.viewport.addViewportAndScissorRect(nvrhi::Viewport(
                static_cast<float>(m_GraphicsDevice.GetBackBufferWidth()),
                static_cast<float>(m_GraphicsDevice.GetBackBufferHeight())));
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
                                  uint32_t indexCount,
                                  uint32_t indexOffset)
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
            state.viewport.addViewportAndScissorRect(nvrhi::Viewport(
                static_cast<float>(m_GraphicsDevice.GetBackBufferWidth()),
                static_cast<float>(m_GraphicsDevice.GetBackBufferHeight())));
        }

        commandList->setGraphicsState(state);

        nvrhi::DrawArguments drawArgs;
        drawArgs.vertexCount = indexCount;
        drawArgs.startIndexLocation = indexOffset;
        commandList->drawIndexed(drawArgs);

        m_Stats.DrawCalls++;
        m_Stats.IndicesSubmitted += indexCount;
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
        nvrhi::ITexture* backBuffer = m_GraphicsDevice.GetCurrentBackBuffer();

        if (!nvrhiDevice || !backBuffer)
        {
            m_Impl->CurrentFramebuffer = nullptr;
            return;
        }

        nvrhi::FramebufferDesc fbDesc;
        fbDesc.addColorAttachment(backBuffer);

        m_Impl->CurrentFramebuffer = nvrhiDevice->createFramebuffer(fbDesc);
    }
}
