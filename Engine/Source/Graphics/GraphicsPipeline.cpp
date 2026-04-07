#include "Core/LifePCH.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/Shader.h"
#include "Graphics/GraphicsTypesInternal.h"
#include "Core/Log.h"

#include <nvrhi/nvrhi.h>

namespace Life
{
    struct GraphicsPipeline::Impl
    {
        nvrhi::GraphicsPipelineHandle Pipeline;
        nvrhi::InputLayoutHandle InputLayout;
        nvrhi::BindingLayoutHandle BindingLayout;
    };

    GraphicsPipeline::~GraphicsPipeline() = default;

    GraphicsPipeline::GraphicsPipeline(GraphicsPipeline&& other) noexcept
        : m_Impl(std::move(other.m_Impl))
        , m_Description(std::move(other.m_Description))
    {
    }

    GraphicsPipeline& GraphicsPipeline::operator=(GraphicsPipeline&& other) noexcept
    {
        if (this != &other)
        {
            m_Impl = std::move(other.m_Impl);
            m_Description = std::move(other.m_Description);
        }
        return *this;
    }

    bool GraphicsPipeline::IsValid() const noexcept
    {
        return m_Impl && m_Impl->Pipeline;
    }

    nvrhi::IGraphicsPipeline* GraphicsPipeline::GetNativePipelineHandle() const
    {
        return m_Impl ? m_Impl->Pipeline.Get() : nullptr;
    }

    nvrhi::IInputLayout* GraphicsPipeline::GetNativeInputLayoutHandle() const
    {
        return m_Impl ? m_Impl->InputLayout.Get() : nullptr;
    }

    nvrhi::IBindingLayout* GraphicsPipeline::GetNativeBindingLayoutHandle() const
    {
        return m_Impl ? m_Impl->BindingLayout.Get() : nullptr;
    }

    Scope<GraphicsPipeline> GraphicsPipeline::Create(GraphicsDevice& device,
                                                      const GraphicsPipelineDescription& desc,
                                                      nvrhi::IFramebuffer* framebuffer)
    {
        nvrhi::IDevice* nvrhiDevice = device.GetNvrhiDevice();
        if (!nvrhiDevice)
            return nullptr;

        if (!desc.VertexShader || !desc.VertexShader->IsValid())
        {
            LOG_CORE_ERROR("GraphicsPipeline::Create: Vertex shader is required for '{}'.", desc.DebugName);
            return nullptr;
        }

        if (!desc.PixelShader || !desc.PixelShader->IsValid())
        {
            LOG_CORE_ERROR("GraphicsPipeline::Create: Pixel shader is required for '{}'.", desc.DebugName);
            return nullptr;
        }

        if (!framebuffer)
        {
            LOG_CORE_ERROR("GraphicsPipeline::Create: Framebuffer is required for '{}'.", desc.DebugName);
            return nullptr;
        }

        // Build vertex attribute descriptions from the Life vertex layout
        const auto& attributes = desc.Layout.GetAttributes();
        std::vector<nvrhi::VertexAttributeDesc> nvrhiAttributes;
        nvrhiAttributes.reserve(attributes.size());

        for (uint32_t i = 0; i < static_cast<uint32_t>(attributes.size()); ++i)
        {
            const auto& attr = attributes[i];
            nvrhi::VertexAttributeDesc nvrhiAttr;
            nvrhiAttr.name = attr.Name.c_str();
            nvrhiAttr.format = Internal::ToNvrhiFormat(attr.Format);
            nvrhiAttr.offset = attr.Offset;
            nvrhiAttr.bufferIndex = attr.BufferIndex;
            nvrhiAttr.arraySize = 1;
            nvrhiAttr.elementStride = desc.Layout.GetStride();
            nvrhiAttr.isInstanced = false;
            nvrhiAttributes.push_back(nvrhiAttr);
        }

        nvrhi::InputLayoutHandle inputLayout;
        if (!nvrhiAttributes.empty())
        {
            inputLayout = nvrhiDevice->createInputLayout(
                nvrhiAttributes.data(),
                static_cast<uint32_t>(nvrhiAttributes.size()),
                desc.VertexShader->GetNativeHandle());

            if (!inputLayout)
            {
                LOG_CORE_ERROR("GraphicsPipeline::Create: Failed to create input layout for '{}'.", desc.DebugName);
                return nullptr;
            }
        }

        // Create a binding layout for constant buffers and textures
        nvrhi::BindingLayoutHandle bindingLayout;
        if (desc.UseSceneConstants || desc.UseTextureBinding)
        {
            nvrhi::BindingLayoutDesc bindingLayoutDesc;
            bindingLayoutDesc.visibility = nvrhi::ShaderType::AllGraphics;
            bindingLayoutDesc.registerSpace = 0;
            bindingLayoutDesc.registerSpaceIsDescriptorSet = true;

            nvrhi::VulkanBindingOffsets bindingOffsets;
            if (desc.UseSceneConstants)
            {
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
                bindingOffsets.constantBuffer = 0;
            }

            if (desc.UseTextureBinding)
            {
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
                bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));
                bindingOffsets.shaderResource = desc.UseSceneConstants ? 1 : 0;
                bindingOffsets.sampler = desc.UseSceneConstants ? 2 : 1;
            }

            bindingLayoutDesc.bindingOffsets = bindingOffsets;

            bindingLayout = nvrhiDevice->createBindingLayout(bindingLayoutDesc);
            if (!bindingLayout)
            {
                LOG_CORE_ERROR("GraphicsPipeline::Create: Failed to create binding layout for '{}'.", desc.DebugName);
                return nullptr;
            }
        }

        // Build the pipeline description
        nvrhi::GraphicsPipelineDesc pipelineDesc;
        pipelineDesc.VS = desc.VertexShader->GetNativeHandle();
        pipelineDesc.PS = desc.PixelShader->GetNativeHandle();
        pipelineDesc.inputLayout = inputLayout;
        pipelineDesc.primType = Internal::ToNvrhiPrimitiveType(desc.Topology);
        pipelineDesc.renderState.rasterState = Internal::ToNvrhiRasterState(desc.Rasterizer);
        pipelineDesc.renderState.depthStencilState = Internal::ToNvrhiDepthStencilState(desc.DepthStencil);
        if (bindingLayout)
            pipelineDesc.addBindingLayout(bindingLayout);

        nvrhi::BlendState blendState;
        blendState.targets[0] = Internal::ToNvrhiBlendRenderTarget(desc.Blend);
        pipelineDesc.renderState.blendState = blendState;

        nvrhi::FramebufferInfoEx fbInfo = framebuffer->getFramebufferInfo();
        nvrhi::GraphicsPipelineHandle pipeline = nvrhiDevice->createGraphicsPipeline(pipelineDesc, fbInfo);
        if (!pipeline)
        {
            LOG_CORE_ERROR("GraphicsPipeline::Create: Failed to create graphics pipeline '{}'.", desc.DebugName);
            return nullptr;
        }

        Scope<GraphicsPipeline> result(new GraphicsPipeline());
        result->m_Impl = CreateScope<Impl>();
        result->m_Impl->Pipeline = pipeline;
        result->m_Impl->InputLayout = inputLayout;
        result->m_Impl->BindingLayout = bindingLayout;
        result->m_Description = desc;
        return result;
    }
}
