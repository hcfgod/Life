#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Graphics/VertexLayout.h"
#include "Core/Memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nvrhi
{
    class IGraphicsPipeline;
    class IInputLayout;
    class IFramebuffer;
    class IBindingLayout;
}

namespace Life
{
    class GraphicsDevice;
    class Shader;

    struct GraphicsPipelineDescription
    {
        std::string DebugName;
        Shader* VertexShader = nullptr;
        Shader* PixelShader = nullptr;
        VertexLayout Layout;
        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        RasterizerState Rasterizer;
        DepthStencilState DepthStencil;
        BlendState Blend;
        std::vector<TextureFormat> RenderTargetFormats;
        TextureFormat DepthFormat = TextureFormat::Unknown;
    };

    class GraphicsPipeline
    {
    public:
        ~GraphicsPipeline();

        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
        GraphicsPipeline(GraphicsPipeline&&) noexcept;
        GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept;

        const GraphicsPipelineDescription& GetDescription() const noexcept { return m_Description; }
        bool IsValid() const noexcept;

        nvrhi::IGraphicsPipeline* GetNativePipelineHandle() const;
        nvrhi::IInputLayout* GetNativeInputLayoutHandle() const;
        nvrhi::IBindingLayout* GetNativeBindingLayoutHandle() const;

        static Scope<GraphicsPipeline> Create(GraphicsDevice& device, const GraphicsPipelineDescription& desc,
                                               nvrhi::IFramebuffer* framebuffer);

    private:
        friend class Renderer;

        GraphicsPipeline() = default;

        struct Impl;
        Scope<Impl> m_Impl;
        GraphicsPipelineDescription m_Description;
    };
}
