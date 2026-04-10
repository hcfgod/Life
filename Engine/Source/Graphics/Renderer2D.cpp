#include "Core/LifePCH.h"
#include "Graphics/Renderer2D.h"

#include "Assets/TextureAsset.h"
#include "Core/Log.h"
#include "Graphics/Camera.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/RenderCommand.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shader.h"
#include "Graphics/ShaderLibrary.h"
#include "Graphics/TextureResource.h"
#include "Graphics/VertexLayout.h"
#include "Platform/PlatformDetection.h"

#include <algorithm>
#include <array>
#include <glm/ext/matrix_transform.hpp>

namespace Life
{
    namespace
    {
        constexpr uint32_t MaxQuads = 16384;
        constexpr uint32_t BufferVersionCount = 4;
        constexpr uint32_t StaticQuadVertexCount = 6;

        struct Renderer2DSceneConstants
        {
            glm::mat4 ViewProjection{ 1.0f };
        };

        struct QuadBatchRange
        {
            const TextureResource* Texture = nullptr;
            uint32_t InstanceOffset = 0;
            uint32_t InstanceCount = 0;
        };

        struct QuadStaticVertex
        {
            glm::vec2 LocalPosition{ 0.0f, 0.0f };
            glm::vec2 LocalTexCoord{ 0.0f, 0.0f };
        };

        struct QuadInstanceData
        {
            glm::vec3 QuadPosition{ 0.0f, 0.0f, 0.0f };
            float QuadRotation = 0.0f;
            glm::vec2 QuadSize{ 0.0f, 0.0f };
            glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
            glm::vec4 TexRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        };

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

    struct Renderer2D::Impl
    {
        Scope<GraphicsBuffer> QuadVertexBuffer;
        std::vector<Scope<GraphicsBuffer>> InstanceBuffers;
        std::vector<Scope<GraphicsBuffer>> SceneConstantBuffers;
        Scope<GraphicsPipeline> Pipeline;
        Shader* VertexShader = nullptr;
        Shader* PixelShader = nullptr;
        VertexLayout Layout;
        std::vector<QuadInstanceData> Instances;
        std::vector<QuadBatchRange> Batches;
        Statistics Stats{};
        Scope<TextureResource> WhiteTexture;
        Scope<TextureResource> ErrorTexture;
        GraphicsBuffer* ActiveInstanceBuffer = nullptr;
        GraphicsBuffer* ActiveSceneConstantBuffer = nullptr;
        uint32_t ActiveBufferVersion = BufferVersionCount - 1u;
        uint32_t QueuedQuadCount = 0;
        bool ResourcesReady = false;
        bool ReportedInitializationFailure = false;
        bool SceneActive = false;
    };

    Renderer2D::Renderer2D(Renderer& renderer)
        : m_Renderer(renderer)
        , m_Impl(CreateScope<Impl>())
    {
        m_Impl->Instances.reserve(static_cast<decltype(m_Impl->Instances)::size_type>(MaxQuads));
        m_Impl->Stats = {};
    }

    Renderer2D::~Renderer2D() = default;

    void Renderer2D::BeginScene(const Camera& camera)
    {
        if (!EnsureResourcesReady())
            return;

        const FramebufferExtent framebufferExtent = m_Renderer.GetFramebufferExtent();
        const Viewport viewport = camera.GetPixelViewport(framebufferExtent);

        RenderCommand::SetViewport(m_Renderer, viewport.X, viewport.Y, viewport.Width, viewport.Height);
        RenderCommand::SetScissor(m_Renderer,
                                  static_cast<int32_t>(viewport.X),
                                  static_cast<int32_t>(viewport.Y),
                                  static_cast<uint32_t>(viewport.Width),
                                  static_cast<uint32_t>(viewport.Height));

        if (camera.GetClearMode() == CameraClearMode::SolidColor)
        {
            const auto& clearColor = camera.GetClearColor();
            RenderCommand::Clear(m_Renderer, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        }

        BeginScene(camera.GetViewProjectionMatrix());
    }

    void Renderer2D::BeginScene(const glm::mat4& viewProjection)
    {
        if (!EnsureResourcesReady())
        {
            m_Impl->SceneActive = false;
            return;
        }

        m_Impl->ActiveBufferVersion = (m_Impl->ActiveBufferVersion + 1u) % static_cast<uint32_t>(m_Impl->InstanceBuffers.size());
        m_Impl->ActiveInstanceBuffer = m_Impl->InstanceBuffers[m_Impl->ActiveBufferVersion].get();
        m_Impl->ActiveSceneConstantBuffer = m_Impl->SceneConstantBuffers[m_Impl->ActiveBufferVersion].get();

        ResetStats();

        if (!UpdateSceneConstants(viewProjection))
        {
            m_Impl->SceneActive = false;
            return;
        }

        ResetQueuedDraws();
        m_Impl->SceneActive = true;
    }

    void Renderer2D::EndScene()
    {
        if (!m_Impl->SceneActive)
            return;

        Flush();
        m_Impl->SceneActive = false;
    }

    void Renderer2D::Flush()
    {
        if (!m_Impl->SceneActive || m_Impl->QueuedQuadCount == 0)
            return;

        SubmitQueuedDraws();
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, m_Impl->WhiteTexture.get(), color);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const TextureResource* texture,
                              const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const TextureResource* texture,
                              const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, texture, color);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                              const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, textureAsset, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                              const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, textureAsset, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, m_Impl->WhiteTexture.get(), color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const TextureResource* texture, const glm::vec4& color)
    {
        if (!m_Impl->SceneActive)
            return;

        PushQuad(position, size, rotationRadians, color, { 0.0f, 0.0f }, { 1.0f, 1.0f }, texture);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const Assets::TextureAsset& textureAsset, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, textureAsset.TryGetTextureResource(), color);
    }

    bool Renderer2D::IsSceneActive() const noexcept
    {
        return m_Impl->SceneActive;
    }

    const Renderer2D::Statistics& Renderer2D::GetStats() const noexcept
    {
        return m_Impl->Stats;
    }

    void Renderer2D::ResetStats() noexcept
    {
        m_Impl->Stats = {};
    }

    bool Renderer2D::EnsureResourcesReady()
    {
        if (m_Impl->ResourcesReady)
        {
            const bool resourcesStillValid =
                m_Impl->QuadVertexBuffer != nullptr && m_Impl->QuadVertexBuffer->IsValid() &&
                m_Impl->InstanceBuffers.size() == BufferVersionCount &&
                m_Impl->SceneConstantBuffers.size() == BufferVersionCount &&
                std::all_of(m_Impl->InstanceBuffers.begin(), m_Impl->InstanceBuffers.end(), [](const Scope<GraphicsBuffer>& buffer) { return buffer != nullptr && buffer->IsValid(); }) &&
                std::all_of(m_Impl->SceneConstantBuffers.begin(), m_Impl->SceneConstantBuffers.end(), [](const Scope<GraphicsBuffer>& buffer) { return buffer != nullptr && buffer->IsValid(); }) &&
                m_Impl->ActiveInstanceBuffer != nullptr && m_Impl->ActiveInstanceBuffer->IsValid() &&
                m_Impl->ActiveSceneConstantBuffer != nullptr && m_Impl->ActiveSceneConstantBuffer->IsValid() &&
                m_Impl->Pipeline != nullptr && m_Impl->Pipeline->IsValid() &&
                m_Impl->WhiteTexture != nullptr && m_Impl->WhiteTexture->IsValid() &&
                m_Impl->ErrorTexture != nullptr && m_Impl->ErrorTexture->IsValid();

            if (resourcesStillValid)
                return true;

            InvalidateResources();
        }

        GraphicsDevice& device = m_Renderer.GetGraphicsDevice();
        const std::array<QuadStaticVertex, StaticQuadVertexCount> quadVertices =
        {
            QuadStaticVertex{ { -0.5f, -0.5f }, { 0.0f, 1.0f } },
            QuadStaticVertex{ {  0.5f, -0.5f }, { 1.0f, 1.0f } },
            QuadStaticVertex{ {  0.5f,  0.5f }, { 1.0f, 0.0f } },
            QuadStaticVertex{ {  0.5f,  0.5f }, { 1.0f, 0.0f } },
            QuadStaticVertex{ { -0.5f,  0.5f }, { 0.0f, 0.0f } },
            QuadStaticVertex{ { -0.5f, -0.5f }, { 0.0f, 1.0f } }
        };

        VertexBufferSpecification quadVertexBufferSpecification;
        quadVertexBufferSpecification.SizeInBytes = static_cast<uint32_t>(sizeof(quadVertices));
        quadVertexBufferSpecification.Stride = static_cast<uint32_t>(sizeof(QuadStaticVertex));
        quadVertexBufferSpecification.DebugName = "Renderer2DQuadVertexBuffer";

        VertexBufferSpecification instanceBufferSpecification;
        instanceBufferSpecification.SizeInBytes = static_cast<uint32_t>(MaxQuads * sizeof(QuadInstanceData));
        instanceBufferSpecification.Stride = static_cast<uint32_t>(sizeof(QuadInstanceData));
        instanceBufferSpecification.DebugName = "Renderer2DInstanceBuffer";

        m_Impl->Layout = VertexLayout
        {
            { "inLocalPosition", VertexAttributeSemantic::Position, TextureFormat::RG32_FLOAT, 0, 0, VertexInputRate::PerVertex },
            { "inLocalTexCoord", VertexAttributeSemantic::TexCoord0, TextureFormat::RG32_FLOAT, 0, 0, VertexInputRate::PerVertex },
            { "inQuadTransform", VertexAttributeSemantic::Custom, TextureFormat::RGBA32_FLOAT, 0, 1, VertexInputRate::PerInstance },
            { "inQuadSize", VertexAttributeSemantic::Custom, TextureFormat::RG32_FLOAT, 0, 1, VertexInputRate::PerInstance },
            { "inColor", VertexAttributeSemantic::Color, TextureFormat::RGBA32_FLOAT, 0, 1, VertexInputRate::PerInstance },
            { "inTexRect", VertexAttributeSemantic::TexCoord1, TextureFormat::RGBA32_FLOAT, 0, 1, VertexInputRate::PerInstance }
        };

        if (!m_Impl->QuadVertexBuffer)
            m_Impl->QuadVertexBuffer = GraphicsBuffer::CreateVertex(device, quadVertices.data(), quadVertexBufferSpecification);

        if (m_Impl->InstanceBuffers.empty())
        {
            m_Impl->InstanceBuffers.reserve(BufferVersionCount);
            for (uint32_t bufferIndex = 0; bufferIndex < BufferVersionCount; ++bufferIndex)
            {
                VertexBufferSpecification versionedInstanceBufferSpecification = instanceBufferSpecification;
                versionedInstanceBufferSpecification.DebugName = "Renderer2DInstanceBuffer" + std::to_string(bufferIndex);
                m_Impl->InstanceBuffers.push_back(GraphicsBuffer::CreateDynamicVertex(device, versionedInstanceBufferSpecification));
            }
        }

        if (m_Impl->SceneConstantBuffers.empty())
        {
            m_Impl->SceneConstantBuffers.reserve(BufferVersionCount);
            for (uint32_t bufferIndex = 0; bufferIndex < BufferVersionCount; ++bufferIndex)
                m_Impl->SceneConstantBuffers.push_back(GraphicsBuffer::CreateConstant(device, sizeof(Renderer2DSceneConstants), "Renderer2DSceneConstants" + std::to_string(bufferIndex)));
        }

        if (m_Impl->ActiveInstanceBuffer == nullptr && !m_Impl->InstanceBuffers.empty())
            m_Impl->ActiveInstanceBuffer = m_Impl->InstanceBuffers.front().get();

        if (m_Impl->ActiveSceneConstantBuffer == nullptr && !m_Impl->SceneConstantBuffers.empty())
            m_Impl->ActiveSceneConstantBuffer = m_Impl->SceneConstantBuffers.front().get();

        if (!m_Impl->WhiteTexture)
            m_Impl->WhiteTexture = CreateSolidTexture(device, "Renderer2DWhiteTexture", { 255, 255, 255, 255 });

        if (!m_Impl->ErrorTexture)
            m_Impl->ErrorTexture = CreateSolidTexture(device, "Renderer2DErrorTexture", { 255, 0, 255, 255 });

        const std::filesystem::path executablePath = PlatformDetection::GetExecutablePath();
        const std::filesystem::path shaderDirectory = executablePath.parent_path() / "Assets" / "Shaders";
        const std::filesystem::path vertexShaderPath = shaderDirectory / "Renderer2D.vert.spv";
        const std::filesystem::path pixelShaderPath = shaderDirectory / "Renderer2D.frag.spv";
        ShaderLibrary& shaderLibrary = m_Renderer.GetShaderLibrary();

        m_Impl->VertexShader = shaderLibrary.Get("Renderer2D.Vertex");
        if (!m_Impl->VertexShader && std::filesystem::exists(vertexShaderPath))
        {
            ShaderDescription description;
            description.DebugName = "Renderer2DVertexShader";
            description.Stage = ShaderStage::Vertex;
            m_Impl->VertexShader = shaderLibrary.LoadFromFile(
                "Renderer2D.Vertex",
                description,
                vertexShaderPath.string());
        }

        m_Impl->PixelShader = shaderLibrary.Get("Renderer2D.Pixel");
        if (!m_Impl->PixelShader && std::filesystem::exists(pixelShaderPath))
        {
            ShaderDescription description;
            description.DebugName = "Renderer2DPixelShader";
            description.Stage = ShaderStage::Pixel;
            m_Impl->PixelShader = shaderLibrary.LoadFromFile(
                "Renderer2D.Pixel",
                description,
                pixelShaderPath.string());
        }

        if (m_Impl->VertexShader && m_Impl->PixelShader && !m_Impl->Pipeline)
        {
            GraphicsPipelineDescription pipelineDescription;
            pipelineDescription.DebugName = "Renderer2DPipeline";
            pipelineDescription.VertexShader = m_Impl->VertexShader;
            pipelineDescription.PixelShader = m_Impl->PixelShader;
            pipelineDescription.Layout = m_Impl->Layout;
            pipelineDescription.Topology = PrimitiveTopology::TriangleList;
            pipelineDescription.Rasterizer.Cull = CullMode::None;
            pipelineDescription.DepthStencil.DepthTestEnable = false;
            pipelineDescription.DepthStencil.DepthWriteEnable = false;
            pipelineDescription.Blend.BlendEnable = true;
            pipelineDescription.Blend.SrcColorFactor = BlendFactor::SrcAlpha;
            pipelineDescription.Blend.DstColorFactor = BlendFactor::InvSrcAlpha;
            pipelineDescription.Blend.SrcAlphaFactor = BlendFactor::One;
            pipelineDescription.Blend.DstAlphaFactor = BlendFactor::InvSrcAlpha;
            pipelineDescription.UseSceneConstants = true;
            pipelineDescription.UseTextureBinding = true;

            m_Impl->Pipeline = m_Renderer.CreatePipeline(pipelineDescription);
        }

        m_Impl->ResourcesReady =
            m_Impl->QuadVertexBuffer != nullptr &&
            m_Impl->InstanceBuffers.size() == BufferVersionCount &&
            m_Impl->SceneConstantBuffers.size() == BufferVersionCount &&
            std::all_of(m_Impl->InstanceBuffers.begin(), m_Impl->InstanceBuffers.end(), [](const Scope<GraphicsBuffer>& buffer) { return buffer != nullptr && buffer->IsValid(); }) &&
            std::all_of(m_Impl->SceneConstantBuffers.begin(), m_Impl->SceneConstantBuffers.end(), [](const Scope<GraphicsBuffer>& buffer) { return buffer != nullptr && buffer->IsValid(); }) &&
            m_Impl->ActiveInstanceBuffer != nullptr && m_Impl->ActiveInstanceBuffer->IsValid() &&
            m_Impl->ActiveSceneConstantBuffer != nullptr && m_Impl->ActiveSceneConstantBuffer->IsValid() &&
            m_Impl->Pipeline != nullptr && m_Impl->Pipeline->IsValid() &&
            m_Impl->VertexShader != nullptr &&
            m_Impl->PixelShader != nullptr &&
            m_Impl->WhiteTexture != nullptr && m_Impl->WhiteTexture->IsValid() &&
            m_Impl->ErrorTexture != nullptr && m_Impl->ErrorTexture->IsValid();

        if (!m_Impl->ResourcesReady)
        {
            if (!m_Impl->ReportedInitializationFailure)
            {
                LOG_CORE_ERROR(
                    "Renderer2D failed to initialize required resources (QuadVertexBuffer={}, InstanceBuffers={}, SceneConstants={}, Pipeline={}, VertexShader={}, PixelShader={}, WhiteTexture={}, ErrorTexture={}).",
                    m_Impl->QuadVertexBuffer != nullptr,
                    m_Impl->InstanceBuffers.size() == BufferVersionCount,
                    m_Impl->SceneConstantBuffers.size() == BufferVersionCount,
                    m_Impl->Pipeline != nullptr,
                    m_Impl->VertexShader != nullptr,
                    m_Impl->PixelShader != nullptr,
                    m_Impl->WhiteTexture != nullptr,
                    m_Impl->ErrorTexture != nullptr);
                m_Impl->ReportedInitializationFailure = true;
            }

            return false;
        }

        m_Impl->ReportedInitializationFailure = false;
        return true;
    }

    void Renderer2D::InvalidateResources() noexcept
    {
        m_Impl->ResourcesReady = false;
        m_Impl->SceneActive = false;
        ResetQueuedDraws();
        m_Impl->QuadVertexBuffer.reset();
        m_Impl->InstanceBuffers.clear();
        m_Impl->SceneConstantBuffers.clear();
        m_Impl->Pipeline.reset();
        m_Impl->WhiteTexture.reset();
        m_Impl->ErrorTexture.reset();
        m_Impl->ActiveInstanceBuffer = nullptr;
        m_Impl->ActiveSceneConstantBuffer = nullptr;
        m_Impl->ActiveBufferVersion = BufferVersionCount - 1u;
        m_Impl->VertexShader = nullptr;
        m_Impl->PixelShader = nullptr;
    }

    bool Renderer2D::UpdateSceneConstants(const glm::mat4& viewProjection)
    {
        if (!m_Impl->ActiveSceneConstantBuffer)
            return false;

        Renderer2DSceneConstants sceneConstants;
        sceneConstants.ViewProjection = viewProjection;
        if (!m_Impl->ActiveSceneConstantBuffer->SetData(m_Renderer.GetGraphicsDevice(), &sceneConstants, sizeof(sceneConstants)))
        {
            LOG_CORE_ERROR("Renderer2D failed to upload scene constant data.");
            InvalidateResources();
            return false;
        }

        return true;
    }

    void Renderer2D::ResetQueuedDraws() noexcept
    {
        m_Impl->Instances.clear();
        m_Impl->Batches.clear();
        m_Impl->QueuedQuadCount = 0;
    }

    void Renderer2D::SubmitQueuedDraws()
    {
        if (!m_Impl->Pipeline || !m_Impl->QuadVertexBuffer || !m_Impl->ActiveInstanceBuffer || !m_Impl->ActiveSceneConstantBuffer || m_Impl->Batches.empty() || m_Impl->Instances.empty())
        {
            ResetQueuedDraws();
            return;
        }

        const uint32_t instanceDataSize = static_cast<uint32_t>(m_Impl->Instances.size() * sizeof(QuadInstanceData));
        if (!m_Impl->ActiveInstanceBuffer->SetData(m_Renderer.GetGraphicsDevice(), m_Impl->Instances.data(), instanceDataSize))
        {
            LOG_CORE_ERROR("Renderer2D failed to upload queued instance data.");
            InvalidateResources();
            return;
        }

        for (const QuadBatchRange& batch : m_Impl->Batches)
        {
            DrawParameters drawParameters;
            drawParameters.VertexCount = StaticQuadVertexCount;
            drawParameters.InstanceCount = batch.InstanceCount;
            drawParameters.InstanceOffset = batch.InstanceOffset;

            RenderCommand::Draw(m_Renderer,
                                *m_Impl->Pipeline,
                                {
                                    VertexBufferBindingView{ m_Impl->QuadVertexBuffer.get(), 0, 0 },
                                    VertexBufferBindingView{ m_Impl->ActiveInstanceBuffer, 1, 0 }
                                },
                                drawParameters,
                                batch.Texture,
                                m_Impl->ActiveSceneConstantBuffer);
        }

        m_Impl->Stats.DrawCalls += static_cast<uint32_t>(m_Impl->Batches.size());
        m_Impl->Stats.QuadCount += m_Impl->QueuedQuadCount;
        ResetQueuedDraws();
    }

    void Renderer2D::PushQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians, const glm::vec4& color,
                              const glm::vec2& uvMin, const glm::vec2& uvMax, const TextureResource* texture)
    {
        const TextureResource* resolvedTexture = texture != nullptr ? texture : m_Impl->ErrorTexture.get();
        if (resolvedTexture == nullptr)
            return;

        if (m_Impl->QueuedQuadCount >= MaxQuads)
            Flush();

        if (m_Impl->Batches.empty() || m_Impl->Batches.back().Texture != resolvedTexture)
        {
            QuadBatchRange batch;
            batch.Texture = resolvedTexture;
            batch.InstanceOffset = static_cast<uint32_t>(m_Impl->Instances.size());
            m_Impl->Batches.push_back(batch);
        }

        QuadInstanceData instance;
        instance.QuadPosition = position;
        instance.QuadRotation = rotationRadians;
        instance.QuadSize = size;
        instance.Color = color;
        instance.TexRect = { uvMin.x, uvMin.y, uvMax.x, uvMax.y };
        m_Impl->Instances.push_back(instance);

        QuadBatchRange& batch = m_Impl->Batches.back();
        batch.InstanceCount++;
        m_Impl->QueuedQuadCount++;
    }
}
