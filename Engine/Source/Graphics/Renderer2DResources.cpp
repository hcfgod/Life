#include "Core/LifePCH.h"
#include "Graphics/Detail/Renderer2DResources.h"
#include "Graphics/Detail/Renderer2DBatching.h"
#include "Graphics/Detail/Renderer2DDetail.h"
#include "Graphics/Detail/Renderer2DPipeline.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/Renderer.h"

#include "Core/Log.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/TextureResource.h"

#include <algorithm>
#include <array>
#include <string>

namespace Life::Detail
{
    namespace
    {
        Scope<TextureResource> CreateSolidTexture(GraphicsDevice& device, const char* debugName,
                                                  const std::array<uint8_t, 4>& pixel)
        {
            TextureDescription desc;
            desc.DebugName = debugName;
            desc.Width = 1;
            desc.Height = 1;
            desc.MipLevels = 1;
            desc.Format = TextureFormat::RGBA8_UNORM;
            return TextureResource::Create2D(device, desc, pixel.data());
        }
    }

    Renderer2DResources::Renderer2DResources(Renderer2D& renderer2D)
        : m_Renderer2D(renderer2D)
    {
    }

    bool Renderer2DResources::ValidateResourceState() const noexcept
    {
        return m_Renderer2D.m_Impl->QuadVertexBuffer != nullptr &&
               m_Renderer2D.m_Impl->QuadVertexBuffer->IsValid() &&
               m_Renderer2D.m_Impl->InstanceBuffers.size() == Renderer2DBufferVersionCount &&
               m_Renderer2D.m_Impl->SceneConstantBuffers.size() == Renderer2DBufferVersionCount &&
               std::all_of(m_Renderer2D.m_Impl->InstanceBuffers.begin(), m_Renderer2D.m_Impl->InstanceBuffers.end(),
                           [](const Scope<GraphicsBuffer>& buffer) { return buffer != nullptr && buffer->IsValid(); }) &&
               std::all_of(m_Renderer2D.m_Impl->SceneConstantBuffers.begin(), m_Renderer2D.m_Impl->SceneConstantBuffers.end(),
                           [](const Scope<GraphicsBuffer>& buffer) { return buffer != nullptr && buffer->IsValid(); }) &&
               m_Renderer2D.m_Impl->ActiveInstanceBuffer != nullptr &&
               m_Renderer2D.m_Impl->ActiveInstanceBuffer->IsValid() &&
               m_Renderer2D.m_Impl->ActiveSceneConstantBuffer != nullptr &&
               m_Renderer2D.m_Impl->ActiveSceneConstantBuffer->IsValid() &&
               m_Renderer2D.m_Impl->Pipeline != nullptr &&
               m_Renderer2D.m_Impl->Pipeline->IsValid() &&
               m_Renderer2D.m_Impl->VertexShader != nullptr &&
               m_Renderer2D.m_Impl->PixelShader != nullptr &&
               m_Renderer2D.m_Impl->WhiteTexture != nullptr &&
               m_Renderer2D.m_Impl->WhiteTexture->IsValid() &&
               m_Renderer2D.m_Impl->ErrorTexture != nullptr &&
               m_Renderer2D.m_Impl->ErrorTexture->IsValid();
    }

    bool Renderer2DResources::AcquireBootstrapResources()
    {
        GraphicsDevice& device = m_Renderer2D.m_Renderer.GetGraphicsDevice();
        const std::array<Renderer2DQuadStaticVertex, Renderer2DStaticQuadVertexCount> quadVertices =
        {
            Renderer2DQuadStaticVertex{ { -0.5f, -0.5f }, { 0.0f, 1.0f } },
            Renderer2DQuadStaticVertex{ {  0.5f, -0.5f }, { 1.0f, 1.0f } },
            Renderer2DQuadStaticVertex{ {  0.5f,  0.5f }, { 1.0f, 0.0f } },
            Renderer2DQuadStaticVertex{ {  0.5f,  0.5f }, { 1.0f, 0.0f } },
            Renderer2DQuadStaticVertex{ { -0.5f,  0.5f }, { 0.0f, 0.0f } },
            Renderer2DQuadStaticVertex{ { -0.5f, -0.5f }, { 0.0f, 1.0f } }
        };

        VertexBufferSpecification quadVertexBufferSpecification;
        quadVertexBufferSpecification.SizeInBytes = static_cast<uint32_t>(sizeof(quadVertices));
        quadVertexBufferSpecification.Stride = static_cast<uint32_t>(sizeof(Renderer2DQuadStaticVertex));
        quadVertexBufferSpecification.DebugName = "Renderer2DQuadVertexBuffer";

        VertexBufferSpecification instanceBufferSpecification;
        instanceBufferSpecification.SizeInBytes = static_cast<uint32_t>(Renderer2DMaxQuads * sizeof(Renderer2DQuadInstanceData));
        instanceBufferSpecification.Stride = static_cast<uint32_t>(sizeof(Renderer2DQuadInstanceData));
        instanceBufferSpecification.DebugName = "Renderer2DInstanceBuffer";

        m_Renderer2D.m_Impl->Layout = VertexLayout
        {
            { "inLocalPosition", VertexAttributeSemantic::Position, TextureFormat::RG32_FLOAT, 0, 0, VertexInputRate::PerVertex },
            { "inLocalTexCoord", VertexAttributeSemantic::TexCoord0, TextureFormat::RG32_FLOAT, 0, 0, VertexInputRate::PerVertex },
            { "inQuadTransform", VertexAttributeSemantic::Custom, TextureFormat::RGBA32_FLOAT, 0, 1, VertexInputRate::PerInstance },
            { "inQuadSize", VertexAttributeSemantic::Custom, TextureFormat::RG32_FLOAT, 0, 1, VertexInputRate::PerInstance },
            { "inColor", VertexAttributeSemantic::Color, TextureFormat::RGBA32_FLOAT, 0, 1, VertexInputRate::PerInstance },
            { "inTexRect", VertexAttributeSemantic::TexCoord1, TextureFormat::RGBA32_FLOAT, 0, 1, VertexInputRate::PerInstance }
        };

        if (!m_Renderer2D.m_Impl->QuadVertexBuffer)
            m_Renderer2D.m_Impl->QuadVertexBuffer = GraphicsBuffer::CreateVertex(device, quadVertices.data(), quadVertexBufferSpecification);

        if (m_Renderer2D.m_Impl->InstanceBuffers.empty())
        {
            m_Renderer2D.m_Impl->InstanceBuffers.reserve(Renderer2DBufferVersionCount);
            for (uint32_t bufferIndex = 0; bufferIndex < Renderer2DBufferVersionCount; ++bufferIndex)
            {
                VertexBufferSpecification versionedInstanceBufferSpecification = instanceBufferSpecification;
                versionedInstanceBufferSpecification.DebugName = "Renderer2DInstanceBuffer" + std::to_string(bufferIndex);
                m_Renderer2D.m_Impl->InstanceBuffers.push_back(GraphicsBuffer::CreateDynamicVertex(device, versionedInstanceBufferSpecification));
            }
        }

        if (m_Renderer2D.m_Impl->SceneConstantBuffers.empty())
        {
            m_Renderer2D.m_Impl->SceneConstantBuffers.reserve(Renderer2DBufferVersionCount);
            for (uint32_t bufferIndex = 0; bufferIndex < Renderer2DBufferVersionCount; ++bufferIndex)
                m_Renderer2D.m_Impl->SceneConstantBuffers.push_back(GraphicsBuffer::CreateConstant(device, sizeof(Renderer2DSceneConstants), "Renderer2DSceneConstants" + std::to_string(bufferIndex)));
        }

        if (m_Renderer2D.m_Impl->ActiveInstanceBuffer == nullptr && !m_Renderer2D.m_Impl->InstanceBuffers.empty())
            m_Renderer2D.m_Impl->ActiveInstanceBuffer = m_Renderer2D.m_Impl->InstanceBuffers.front().get();

        if (m_Renderer2D.m_Impl->ActiveSceneConstantBuffer == nullptr && !m_Renderer2D.m_Impl->SceneConstantBuffers.empty())
            m_Renderer2D.m_Impl->ActiveSceneConstantBuffer = m_Renderer2D.m_Impl->SceneConstantBuffers.front().get();

        if (!m_Renderer2D.m_Impl->WhiteTexture)
            m_Renderer2D.m_Impl->WhiteTexture = CreateSolidTexture(device, "Renderer2DWhiteTexture", { 255, 255, 255, 255 });

        if (!m_Renderer2D.m_Impl->ErrorTexture)
            m_Renderer2D.m_Impl->ErrorTexture = CreateSolidTexture(device, "Renderer2DErrorTexture", { 255, 0, 255, 255 });

        return m_Renderer2D.m_Impl->QuadVertexBuffer != nullptr &&
               m_Renderer2D.m_Impl->InstanceBuffers.size() == Renderer2DBufferVersionCount &&
               m_Renderer2D.m_Impl->SceneConstantBuffers.size() == Renderer2DBufferVersionCount &&
               m_Renderer2D.m_Impl->WhiteTexture != nullptr &&
               m_Renderer2D.m_Impl->ErrorTexture != nullptr;
    }

    bool Renderer2DResources::EnsureResourcesReady()
    {
        if (m_Renderer2D.m_Impl->ResourcesReady)
        {
            if (ValidateResourceState())
                return true;

            this->InvalidateResources();
        }

        Renderer2DPipeline pipeline(m_Renderer2D);
        const bool bootstrapReady = AcquireBootstrapResources();
        const bool shadersReady = bootstrapReady && pipeline.AcquireShaderResources();
        const bool pipelineReady = shadersReady && pipeline.AcquirePipelineState();

        m_Renderer2D.m_Impl->ResourcesReady = pipelineReady && ValidateResourceState();
        if (!m_Renderer2D.m_Impl->ResourcesReady)
        {
            if (!m_Renderer2D.m_Impl->ReportedInitializationFailure)
            {
                LOG_CORE_ERROR(
                    "Renderer2D failed to initialize required resources (QuadVertexBuffer={}, InstanceBuffers={}, SceneConstants={}, Pipeline={}, VertexShader={}, PixelShader={}, WhiteTexture={}, ErrorTexture={}).",
                    m_Renderer2D.m_Impl->QuadVertexBuffer != nullptr,
                    m_Renderer2D.m_Impl->InstanceBuffers.size() == Renderer2DBufferVersionCount,
                    m_Renderer2D.m_Impl->SceneConstantBuffers.size() == Renderer2DBufferVersionCount,
                    m_Renderer2D.m_Impl->Pipeline != nullptr,
                    m_Renderer2D.m_Impl->VertexShader != nullptr,
                    m_Renderer2D.m_Impl->PixelShader != nullptr,
                    m_Renderer2D.m_Impl->WhiteTexture != nullptr,
                    m_Renderer2D.m_Impl->ErrorTexture != nullptr);
                m_Renderer2D.m_Impl->ReportedInitializationFailure = true;
            }

            return false;
        }

        m_Renderer2D.m_Impl->ReportedInitializationFailure = false;
        return true;
    }

    void Renderer2DResources::InvalidateResources() noexcept
    {
        Renderer2DBatching batching(m_Renderer2D);
        m_Renderer2D.m_Impl->ResourcesReady = false;
        m_Renderer2D.m_Impl->SceneActive = false;
        batching.ResetQueuedDraws();
        m_Renderer2D.m_Impl->QuadVertexBuffer.reset();
        m_Renderer2D.m_Impl->InstanceBuffers.clear();
        m_Renderer2D.m_Impl->SceneConstantBuffers.clear();
        m_Renderer2D.m_Impl->Pipeline.reset();
        m_Renderer2D.m_Impl->WhiteTexture.reset();
        m_Renderer2D.m_Impl->ErrorTexture.reset();
        m_Renderer2D.m_Impl->ActiveInstanceBuffer = nullptr;
        m_Renderer2D.m_Impl->ActiveSceneConstantBuffer = nullptr;
        m_Renderer2D.m_Impl->ActiveBufferVersion = Renderer2DBufferVersionCount - 1u;
        m_Renderer2D.m_Impl->VertexShader = nullptr;
        m_Renderer2D.m_Impl->PixelShader = nullptr;
    }
}
