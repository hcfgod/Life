#include "Core/LifePCH.h"
#include "Graphics/Renderer2D.h"

#include "Core/Log.h"
#include "Graphics/Camera.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/RenderCommand.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shader.h"
#include "Graphics/ShaderLibrary.h"
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
        };
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
        uint32_t QuadCount = 0;
        bool Initialized = false;
        bool InitializationAttempted = false;
        bool SceneActive = false;
    };

    Renderer2D::Renderer2D(Renderer& renderer)
        : m_Renderer(renderer)
        , m_Impl(CreateScope<Impl>())
    {
        m_Impl->Vertices.reserve(MaxQuads * VerticesPerQuad);
    }

    Renderer2D::~Renderer2D() = default;

    void Renderer2D::BeginScene(const Camera& camera)
    {
        InitializeResources();
        if (!m_Impl->Initialized)
            return;

        const Viewport viewport = camera.GetPixelViewport(
            m_Renderer.GetGraphicsDevice().GetBackBufferWidth(),
            m_Renderer.GetGraphicsDevice().GetBackBufferHeight());

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
                            m_Impl->QuadCount * VerticesPerQuad);

        m_Impl->Stats.DrawCalls++;
        m_Impl->Stats.QuadCount += m_Impl->QuadCount;
        m_Impl->Vertices.clear();
        m_Impl->QuadCount = 0;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const glm::vec4& color)
    {
        if (!m_Impl->SceneActive)
            return;

        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
            * glm::rotate(glm::mat4(1.0f), rotationRadians, glm::vec3(0.0f, 0.0f, 1.0f))
            * glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));

        PushQuad(transform, color);
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
        if (m_Impl->Initialized || m_Impl->InitializationAttempted)
            return;

        m_Impl->InitializationAttempted = true;

        GraphicsDevice& device = m_Renderer.GetGraphicsDevice();

        m_Impl->Layout = VertexLayout
        {
            { "inPosition", VertexAttributeSemantic::Position, TextureFormat::RGBA32_FLOAT },
            { "inColor", VertexAttributeSemantic::Color, TextureFormat::RGBA32_FLOAT }
        };

        m_Impl->VertexBuffer = GraphicsBuffer::CreateDynamicVertex(
            device,
            MaxQuads * VerticesPerQuad * sizeof(QuadVertex),
            sizeof(QuadVertex),
            "Renderer2DVertexBuffer");

        const std::filesystem::path executablePath = PlatformDetection::GetExecutablePath();
        const std::filesystem::path shaderDirectory = executablePath.parent_path() / "Assets" / "Shaders";
        ShaderLibrary& shaderLibrary = m_Renderer.GetShaderLibrary();

        m_Impl->VertexShader = shaderLibrary.Get("Renderer2D.Vertex");
        if (!m_Impl->VertexShader)
        {
            ShaderDescription description;
            description.DebugName = "Renderer2DVertexShader";
            description.Stage = ShaderStage::Vertex;
            m_Impl->VertexShader = shaderLibrary.LoadFromFile(
                "Renderer2D.Vertex",
                description,
                (shaderDirectory / "Renderer2D.vert.spv").string());
        }

        m_Impl->PixelShader = shaderLibrary.Get("Renderer2D.Pixel");
        if (!m_Impl->PixelShader)
        {
            ShaderDescription description;
            description.DebugName = "Renderer2DPixelShader";
            description.Stage = ShaderStage::Pixel;
            m_Impl->PixelShader = shaderLibrary.LoadFromFile(
                "Renderer2D.Pixel",
                description,
                (shaderDirectory / "Renderer2D.frag.spv").string());
        }

        m_Impl->Initialized =
            m_Impl->VertexBuffer != nullptr &&
            m_Impl->VertexShader != nullptr &&
            m_Impl->PixelShader != nullptr;

        if (!m_Impl->Initialized)
        {
            LOG_CORE_ERROR(
                "Renderer2D failed to initialize required resources (VertexBuffer={}, VertexShader={}, PixelShader={}).",
                m_Impl->VertexBuffer != nullptr,
                m_Impl->VertexShader != nullptr,
                m_Impl->PixelShader != nullptr);
        }
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

        m_Impl->Pipeline = m_Renderer.CreatePipeline(pipelineDescription);
    }

    void Renderer2D::PushQuad(const glm::mat4& transform, const glm::vec4& color)
    {
        if (m_Impl->QuadCount >= MaxQuads)
            Flush();

        const std::array<glm::vec4, VerticesPerQuad> quadPositions =
        {
            glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f),
            glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f),
            glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f),
            glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f),
            glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f),
            glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f)
        };

        for (const glm::vec4& basePosition : quadPositions)
        {
            QuadVertex vertex;
            vertex.Position = m_Impl->ViewProjection * transform * basePosition;
            vertex.Color = color;
            m_Impl->Vertices.push_back(vertex);
        }

        m_Impl->QuadCount++;
    }
}
