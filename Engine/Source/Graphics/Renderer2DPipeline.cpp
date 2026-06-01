#include "Core/LifePCH.h"
#include "Graphics/Detail/Renderer2DPipeline.h"

#include "Graphics/Detail/Renderer2DDetail.h"

#include "Graphics/Renderer2D.h"
#include "Graphics/Renderer.h"
#include "Graphics/Shader.h"
#include "Graphics/ShaderLibrary.h"
#include "Platform/PlatformDetection.h"

#include <filesystem>

namespace Life::Detail
{
    Renderer2DPipeline::Renderer2DPipeline(Renderer2D& renderer2D)
        : m_Renderer2D(renderer2D)
    {
    }

    bool Renderer2DPipeline::AcquireShaderResources()
    {
        const std::filesystem::path executablePath = PlatformDetection::GetExecutablePath();
        const std::filesystem::path shaderDirectory = executablePath.parent_path() / "Assets" / "Shaders";
        const std::filesystem::path vertexShaderPath = shaderDirectory / "Renderer2D.vert.spv";
        const std::filesystem::path pixelShaderPath = shaderDirectory / "Renderer2D.frag.spv";
        ShaderLibrary& shaderLibrary = m_Renderer2D.m_Renderer.GetShaderLibrary();

        m_Renderer2D.m_Impl->VertexShader = shaderLibrary.Get("Renderer2D.Vertex");
        if (!m_Renderer2D.m_Impl->VertexShader && std::filesystem::exists(vertexShaderPath))
        {
            ShaderDescription description;
            description.DebugName = "Renderer2DVertexShader";
            description.Stage = ShaderStage::Vertex;
            m_Renderer2D.m_Impl->VertexShader = shaderLibrary.LoadFromFile(
                "Renderer2D.Vertex",
                description,
                vertexShaderPath.string());
        }

        m_Renderer2D.m_Impl->PixelShader = shaderLibrary.Get("Renderer2D.Pixel");
        if (!m_Renderer2D.m_Impl->PixelShader && std::filesystem::exists(pixelShaderPath))
        {
            ShaderDescription description;
            description.DebugName = "Renderer2DPixelShader";
            description.Stage = ShaderStage::Pixel;
            m_Renderer2D.m_Impl->PixelShader = shaderLibrary.LoadFromFile(
                "Renderer2D.Pixel",
                description,
                pixelShaderPath.string());
        }

        return m_Renderer2D.m_Impl->VertexShader != nullptr && m_Renderer2D.m_Impl->PixelShader != nullptr;
    }

    bool Renderer2DPipeline::AcquirePipelineState()
    {
        if ((m_Renderer2D.m_Impl->VertexShader == nullptr) || (m_Renderer2D.m_Impl->PixelShader == nullptr))
            return false;

        nvrhi::IFramebuffer* currentFramebuffer = m_Renderer2D.m_Renderer.GetCurrentFramebuffer();
        if (currentFramebuffer == nullptr)
            return false;

        const bool useDepth = m_Renderer2D.m_Impl->DepthTestingEnabled && m_Renderer2D.m_Renderer.GetDepthRenderTarget() != nullptr;

        const auto ensurePipeline =
            [this, currentFramebuffer, useDepth](
                Scope<GraphicsPipeline>& pipeline,
                nvrhi::IFramebuffer*& pipelineFramebuffer,
                bool& pipelineUsesDepth,
                const char* debugName,
                bool opaquePass) -> bool
        {
            if (pipeline &&
                pipeline->IsValid() &&
                pipelineFramebuffer == currentFramebuffer &&
                pipelineUsesDepth == useDepth)
            {
                return true;
            }

            GraphicsPipelineDescription pipelineDescription;
            pipelineDescription.DebugName = debugName;
            pipelineDescription.VertexShader = m_Renderer2D.m_Impl->VertexShader;
            pipelineDescription.PixelShader = m_Renderer2D.m_Impl->PixelShader;
            pipelineDescription.Layout = m_Renderer2D.m_Impl->Layout;
            pipelineDescription.Topology = PrimitiveTopology::TriangleList;
            pipelineDescription.Rasterizer.Cull = CullMode::None;
            pipelineDescription.DepthStencil.DepthTestEnable = useDepth;
            pipelineDescription.DepthStencil.DepthWriteEnable = opaquePass && useDepth;
            pipelineDescription.DepthStencil.DepthCompareOp = CompareOp::LessOrEqual;
            pipelineDescription.Blend.BlendEnable = !opaquePass;
            pipelineDescription.UseSceneConstants = true;
            pipelineDescription.UseTextureBinding = true;

            if (!opaquePass)
            {
                pipelineDescription.Blend.SrcColorFactor = BlendFactor::One;
                pipelineDescription.Blend.DstColorFactor = BlendFactor::InvSrcAlpha;
                pipelineDescription.Blend.SrcAlphaFactor = BlendFactor::One;
                pipelineDescription.Blend.DstAlphaFactor = BlendFactor::InvSrcAlpha;
            }

            pipeline = m_Renderer2D.m_Renderer.CreatePipeline(pipelineDescription);
            pipelineFramebuffer = currentFramebuffer;
            pipelineUsesDepth = useDepth;
            return pipeline != nullptr && pipeline->IsValid();
        };

        return ensurePipeline(
                   m_Renderer2D.m_Impl->OpaquePipeline,
                   m_Renderer2D.m_Impl->OpaquePipelineFramebuffer,
                   m_Renderer2D.m_Impl->OpaquePipelineUsesDepth,
                   "Renderer2DOpaquePipeline",
                   true) &&
               ensurePipeline(
                   m_Renderer2D.m_Impl->TransparentPipeline,
                   m_Renderer2D.m_Impl->TransparentPipelineFramebuffer,
                   m_Renderer2D.m_Impl->TransparentPipelineUsesDepth,
                   "Renderer2DTransparentPipeline",
                   false);
    }
}
