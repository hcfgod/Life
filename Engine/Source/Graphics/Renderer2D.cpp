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

#include <glm/ext/matrix_transform.hpp>

namespace Life
{
    namespace
    {
        constexpr uint32_t MaxQuads = 1000;
        constexpr uint32_t VerticesPerQuad = 6;

        struct QuadVertex
        {
            glm::vec4 Position;
            glm::vec4 Color;
            glm::vec2 TexCoord;
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
        Scope<GraphicsBuffer> VertexBuffer;
        Scope<GraphicsPipeline> Pipeline;
        Shader* VertexShader = nullptr;
        Shader* PixelShader = nullptr;
        VertexLayout Layout;
        std::vector<QuadVertex> Vertices;
        glm::mat4 ViewProjection{ 1.0f };
        Statistics Stats{};
        const TextureResource* BatchTexture = nullptr;
        Scope<TextureResource> WhiteTexture;
        Scope<TextureResource> ErrorTexture;
        uint32_t QuadCount = 0;
        bool Initialized = false;
        bool ReportedInitializationFailure = false;
        bool SceneActive = false;
    };

    Renderer2D::Renderer2D(Renderer& renderer)
        : m_Renderer(renderer)
        , m_Impl(CreateScope<Impl>())
    {
        m_Impl->Vertices.reserve(
            static_cast<decltype(m_Impl->Vertices)::size_type>(MaxQuads)
            * static_cast<decltype(m_Impl->Vertices)::size_type>(VerticesPerQuad));
    }

    Renderer2D::~Renderer2D() = default;

    void Renderer2D::BeginScene(const Camera& camera)
    {
        InitializeResources();
        if (!m_Impl->Initialized)
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
        InitializeResources();
        if (!m_Impl->Initialized)
            return;

        m_Impl->ViewProjection = viewProjection;
        m_Impl->Vertices.clear();
        m_Impl->QuadCount = 0;
        m_Impl->BatchTexture = nullptr;
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
        if (!m_Impl->SceneActive || m_Impl->QuadCount == 0)
            return;

        EnsurePipeline();
        if (!m_Impl->Pipeline || !m_Impl->VertexBuffer)
            return;

        const uint32_t vertexDataSize = static_cast<uint32_t>(m_Impl->Vertices.size() * sizeof(QuadVertex));
        if (!m_Impl->VertexBuffer->SetData(m_Renderer.GetGraphicsDevice(), m_Impl->Vertices.data(), vertexDataSize))
        {
            LOG_CORE_ERROR("Renderer2D failed to upload batched vertex data.");
            return;
        }

        RenderCommand::Draw(m_Renderer,
                            *m_Impl->Pipeline,
                            *m_Impl->VertexBuffer,
                            m_Impl->QuadCount * VerticesPerQuad,
                            m_Impl->BatchTexture);

        m_Impl->Stats.DrawCalls++;
        m_Impl->Stats.QuadCount += m_Impl->QuadCount;
        m_Impl->Vertices.clear();
        m_Impl->QuadCount = 0;
        m_Impl->BatchTexture = nullptr;
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

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
            * glm::rotate(glm::mat4(1.0f), rotationRadians, glm::vec3(0.0f, 0.0f, 1.0f))
            * glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));

        PushQuad(transform, color, { 0.0f, 0.0f }, { 1.0f, 1.0f }, texture);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const Assets::TextureAsset& textureAsset, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, textureAsset.TryGetTextureResource(), color);
    }

    const Renderer2D::Statistics& Renderer2D::GetStats() const noexcept
    {
        return m_Impl->Stats;
    }

    void Renderer2D::ResetStats() noexcept
    {
        m_Impl->Stats = {};
    }

    void Renderer2D::InitializeResources()
    {
        if (m_Impl->Initialized)
            return;

        GraphicsDevice& device = m_Renderer.GetGraphicsDevice();
        const auto maxVertexCount =
            static_cast<decltype(m_Impl->Vertices)::size_type>(MaxQuads)
            * static_cast<decltype(m_Impl->Vertices)::size_type>(VerticesPerQuad);
        VertexBufferSpecification vertexBufferSpecification;
        vertexBufferSpecification.SizeInBytes = static_cast<uint32_t>(maxVertexCount * sizeof(QuadVertex));
        vertexBufferSpecification.Stride = static_cast<uint32_t>(sizeof(QuadVertex));
        vertexBufferSpecification.DebugName = "Renderer2DVertexBuffer";

        m_Impl->Layout = VertexLayout
        {
            { "inPosition", VertexAttributeSemantic::Position, TextureFormat::RGBA32_FLOAT },
            { "inColor", VertexAttributeSemantic::Color, TextureFormat::RGBA32_FLOAT },
            { "inTexCoord", VertexAttributeSemantic::TexCoord0, TextureFormat::RG32_FLOAT }
        };

        if (!m_Impl->VertexBuffer)
            m_Impl->VertexBuffer = GraphicsBuffer::CreateDynamicVertex(device, vertexBufferSpecification);

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

        m_Impl->Initialized =
            m_Impl->VertexBuffer != nullptr &&
            m_Impl->VertexShader != nullptr &&
            m_Impl->PixelShader != nullptr &&
            m_Impl->WhiteTexture != nullptr &&
            m_Impl->ErrorTexture != nullptr;

        if (!m_Impl->Initialized)
        {
            if (!m_Impl->ReportedInitializationFailure)
            {
                LOG_CORE_ERROR(
                    "Renderer2D failed to initialize required resources (VertexBuffer={}, VertexShader={}, PixelShader={}, WhiteTexture={}, ErrorTexture={}).",
                    m_Impl->VertexBuffer != nullptr,
                    m_Impl->VertexShader != nullptr,
                    m_Impl->PixelShader != nullptr,
                    m_Impl->WhiteTexture != nullptr,
                    m_Impl->ErrorTexture != nullptr);
                m_Impl->ReportedInitializationFailure = true;
            }

            return;
        }

        m_Impl->ReportedInitializationFailure = false;
    }

    void Renderer2D::EnsurePipeline()
    {
        if (m_Impl->Pipeline || !m_Impl->Initialized)
            return;

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
        pipelineDescription.UseTextureBinding = true;

        m_Impl->Pipeline = m_Renderer.CreatePipeline(pipelineDescription);
    }

    void Renderer2D::PushQuad(const glm::mat4& transform, const glm::vec4& color,
                              const glm::vec2& uvMin, const glm::vec2& uvMax, const TextureResource* texture)
    {
        const TextureResource* resolvedTexture = texture != nullptr ? texture : m_Impl->ErrorTexture.get();
        if (resolvedTexture == nullptr)
            return;

        if ((m_Impl->BatchTexture != nullptr && m_Impl->BatchTexture != resolvedTexture) || m_Impl->QuadCount >= MaxQuads)
            Flush();

        if (m_Impl->QuadCount != 0 && m_Impl->BatchTexture != resolvedTexture)
            return;

        if (m_Impl->BatchTexture == nullptr)
            m_Impl->BatchTexture = resolvedTexture;

        const std::array<glm::vec4, VerticesPerQuad> quadPositions =
        {
            glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
            glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f),
            glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f),
            glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f),
            glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f),
            glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f)
        };

        const std::array<glm::vec2, VerticesPerQuad> quadTexCoords =
        {
            glm::vec2(uvMin.x, uvMax.y),
            glm::vec2(uvMax.x, uvMax.y),
            glm::vec2(uvMax.x, uvMin.y),
            glm::vec2(uvMax.x, uvMin.y),
            glm::vec2(uvMin.x, uvMin.y),
            glm::vec2(uvMin.x, uvMax.y)
        };

        for (size_t vertexIndex = 0; vertexIndex < quadPositions.size(); ++vertexIndex)
        {
            QuadVertex vertex;
            vertex.Position = m_Impl->ViewProjection * transform * quadPositions[vertexIndex];
            vertex.Color = color;
            vertex.TexCoord = quadTexCoords[vertexIndex];
            m_Impl->Vertices.push_back(vertex);
        }

        m_Impl->QuadCount++;
    }
}
