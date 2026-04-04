#pragma once

#include "Graphics/GraphicsTypes.h"

#include <nvrhi/nvrhi.h>

namespace Life::Internal
{
    nvrhi::Format ToNvrhiFormat(TextureFormat format);
    TextureFormat FromNvrhiFormat(nvrhi::Format format);
    nvrhi::PrimitiveType ToNvrhiPrimitiveType(PrimitiveTopology topology);
    nvrhi::RasterCullMode ToNvrhiCullMode(CullMode mode);
    nvrhi::RasterFillMode ToNvrhiFillMode(FillMode mode);
    nvrhi::ComparisonFunc ToNvrhiComparisonFunc(CompareOp op);
    nvrhi::BlendFactor ToNvrhiBlendFactor(BlendFactor factor);
    nvrhi::BlendOp ToNvrhiBlendOp(BlendOp op);
    nvrhi::RasterState ToNvrhiRasterState(const RasterizerState& state);
    nvrhi::DepthStencilState ToNvrhiDepthStencilState(const DepthStencilState& state);
    nvrhi::BlendState::RenderTarget ToNvrhiBlendRenderTarget(const BlendState& state);
}
