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

        if (m_Renderer2D.m_Impl->Pipeline && m_Renderer2D.m_Impl->Pipeline->IsValid())
            return true;

        GraphicsPipelineDescription pipelineDescription;
        pipelineDescription.DebugName = "Renderer2DPipeline";
        pipelineDescription.VertexShader = m_Renderer2D.m_Impl->VertexShader;
        pipelineDescription.PixelShader = m_Renderer2D.m_Impl->PixelShader;
        pipelineDescription.Layout = m_Renderer2D.m_Impl->Layout;
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

        m_Renderer2D.m_Impl->Pipeline = m_Renderer2D.m_Renderer.CreatePipeline(pipelineDescription);
        return m_Renderer2D.m_Impl->Pipeline != nullptr && m_Renderer2D.m_Impl->Pipeline->IsValid();
    }
}
