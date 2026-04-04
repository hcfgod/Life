#include "Core/LifePCH.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/GraphicsTypesInternal.h"

#include <nvrhi/nvrhi.h>

namespace Life::Internal
{
    nvrhi::Format ToNvrhiFormat(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RGBA8_UNORM:      return nvrhi::Format::RGBA8_UNORM;
        case TextureFormat::RGBA8_SRGB:       return nvrhi::Format::SRGBA8_UNORM;
        case TextureFormat::BGRA8_UNORM:      return nvrhi::Format::BGRA8_UNORM;
        case TextureFormat::BGRA8_SRGB:       return nvrhi::Format::SBGRA8_UNORM;
        case TextureFormat::RGBA16_FLOAT:     return nvrhi::Format::RGBA16_FLOAT;
        case TextureFormat::RGBA32_FLOAT:     return nvrhi::Format::RGBA32_FLOAT;
        case TextureFormat::RG16_FLOAT:       return nvrhi::Format::RG16_FLOAT;
        case TextureFormat::RG32_FLOAT:       return nvrhi::Format::RG32_FLOAT;
        case TextureFormat::R8_UNORM:         return nvrhi::Format::R8_UNORM;
        case TextureFormat::R16_FLOAT:        return nvrhi::Format::R16_FLOAT;
        case TextureFormat::R32_FLOAT:        return nvrhi::Format::R32_FLOAT;
        case TextureFormat::Depth16:          return nvrhi::Format::D16;
        case TextureFormat::Depth24Stencil8:  return nvrhi::Format::D24S8;
        case TextureFormat::Depth32F:         return nvrhi::Format::D32;
        default:                              return nvrhi::Format::UNKNOWN;
        }
    }

    TextureFormat FromNvrhiFormat(nvrhi::Format format)
    {
        switch (format)
        {
        case nvrhi::Format::RGBA8_UNORM:    return TextureFormat::RGBA8_UNORM;
        case nvrhi::Format::SRGBA8_UNORM:   return TextureFormat::RGBA8_SRGB;
        case nvrhi::Format::BGRA8_UNORM:    return TextureFormat::BGRA8_UNORM;
        case nvrhi::Format::SBGRA8_UNORM:   return TextureFormat::BGRA8_SRGB;
        case nvrhi::Format::RGBA16_FLOAT:   return TextureFormat::RGBA16_FLOAT;
        case nvrhi::Format::RGBA32_FLOAT:   return TextureFormat::RGBA32_FLOAT;
        case nvrhi::Format::RG16_FLOAT:     return TextureFormat::RG16_FLOAT;
        case nvrhi::Format::RG32_FLOAT:     return TextureFormat::RG32_FLOAT;
        case nvrhi::Format::R8_UNORM:       return TextureFormat::R8_UNORM;
        case nvrhi::Format::R16_FLOAT:      return TextureFormat::R16_FLOAT;
        case nvrhi::Format::R32_FLOAT:      return TextureFormat::R32_FLOAT;
        case nvrhi::Format::D16:            return TextureFormat::Depth16;
        case nvrhi::Format::D24S8:          return TextureFormat::Depth24Stencil8;
        case nvrhi::Format::D32:            return TextureFormat::Depth32F;
        default:                            return TextureFormat::Unknown;
        }
    }

    nvrhi::PrimitiveType ToNvrhiPrimitiveType(PrimitiveTopology topology)
    {
        switch (topology)
        {
        case PrimitiveTopology::TriangleList:   return nvrhi::PrimitiveType::TriangleList;
        case PrimitiveTopology::TriangleStrip:  return nvrhi::PrimitiveType::TriangleStrip;
        case PrimitiveTopology::LineList:        return nvrhi::PrimitiveType::LineList;
        case PrimitiveTopology::PointList:       return nvrhi::PrimitiveType::PointList;
        default:                                 return nvrhi::PrimitiveType::TriangleList;
        }
    }

    nvrhi::RasterCullMode ToNvrhiCullMode(CullMode mode)
    {
        switch (mode)
        {
        case CullMode::None:   return nvrhi::RasterCullMode::None;
        case CullMode::Front:  return nvrhi::RasterCullMode::Front;
        case CullMode::Back:   return nvrhi::RasterCullMode::Back;
        default:               return nvrhi::RasterCullMode::Back;
        }
    }

    nvrhi::RasterFillMode ToNvrhiFillMode(FillMode mode)
    {
        switch (mode)
        {
        case FillMode::Solid:     return nvrhi::RasterFillMode::Solid;
        case FillMode::Wireframe: return nvrhi::RasterFillMode::Wireframe;
        default:                  return nvrhi::RasterFillMode::Solid;
        }
    }

    nvrhi::ComparisonFunc ToNvrhiComparisonFunc(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never:          return nvrhi::ComparisonFunc::Never;
        case CompareOp::Less:           return nvrhi::ComparisonFunc::Less;
        case CompareOp::Equal:          return nvrhi::ComparisonFunc::Equal;
        case CompareOp::LessOrEqual:    return nvrhi::ComparisonFunc::LessOrEqual;
        case CompareOp::Greater:        return nvrhi::ComparisonFunc::Greater;
        case CompareOp::NotEqual:       return nvrhi::ComparisonFunc::NotEqual;
        case CompareOp::GreaterOrEqual: return nvrhi::ComparisonFunc::GreaterOrEqual;
        case CompareOp::Always:         return nvrhi::ComparisonFunc::Always;
        default:                        return nvrhi::ComparisonFunc::Less;
        }
    }

    nvrhi::BlendFactor ToNvrhiBlendFactor(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:         return nvrhi::BlendFactor::Zero;
        case BlendFactor::One:          return nvrhi::BlendFactor::One;
        case BlendFactor::SrcColor:     return nvrhi::BlendFactor::SrcColor;
        case BlendFactor::InvSrcColor:  return nvrhi::BlendFactor::InvSrcColor;
        case BlendFactor::SrcAlpha:     return nvrhi::BlendFactor::SrcAlpha;
        case BlendFactor::InvSrcAlpha:  return nvrhi::BlendFactor::InvSrcAlpha;
        case BlendFactor::DstColor:     return nvrhi::BlendFactor::DstColor;
        case BlendFactor::InvDstColor:  return nvrhi::BlendFactor::InvDstColor;
        case BlendFactor::DstAlpha:     return nvrhi::BlendFactor::DstAlpha;
        case BlendFactor::InvDstAlpha:  return nvrhi::BlendFactor::InvDstAlpha;
        default:                        return nvrhi::BlendFactor::Zero;
        }
    }

    nvrhi::BlendOp ToNvrhiBlendOp(BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Add:             return nvrhi::BlendOp::Add;
        case BlendOp::Subtract:        return nvrhi::BlendOp::Subtract;
        case BlendOp::ReverseSubtract: return nvrhi::BlendOp::ReverseSubtract;
        case BlendOp::Min:             return nvrhi::BlendOp::Min;
        case BlendOp::Max:             return nvrhi::BlendOp::Max;
        default:                       return nvrhi::BlendOp::Add;
        }
    }

    nvrhi::RasterState ToNvrhiRasterState(const RasterizerState& state)
    {
        nvrhi::RasterState result;
        result.setCullMode(ToNvrhiCullMode(state.Cull));
        result.setFillMode(ToNvrhiFillMode(state.Fill));
        result.frontCounterClockwise = state.FrontCounterClockwise;
        result.depthBias = state.DepthBias;
        result.depthBiasClamp = state.DepthBiasClamp;
        result.slopeScaledDepthBias = state.SlopeScaledDepthBias;
        return result;
    }

    nvrhi::DepthStencilState ToNvrhiDepthStencilState(const DepthStencilState& state)
    {
        nvrhi::DepthStencilState result;
        result.depthTestEnable = state.DepthTestEnable;
        result.depthWriteEnable = state.DepthWriteEnable;
        result.depthFunc = ToNvrhiComparisonFunc(state.DepthCompareOp);
        result.stencilEnable = state.StencilEnable;
        return result;
    }

    nvrhi::BlendState::RenderTarget ToNvrhiBlendRenderTarget(const BlendState& state)
    {
        nvrhi::BlendState::RenderTarget result;
        result.blendEnable = state.BlendEnable;
        result.srcBlend = ToNvrhiBlendFactor(state.SrcColorFactor);
        result.destBlend = ToNvrhiBlendFactor(state.DstColorFactor);
        result.blendOp = ToNvrhiBlendOp(state.ColorOp);
        result.srcBlendAlpha = ToNvrhiBlendFactor(state.SrcAlphaFactor);
        result.destBlendAlpha = ToNvrhiBlendFactor(state.DstAlphaFactor);
        result.blendOpAlpha = ToNvrhiBlendOp(state.AlphaOp);
        return result;
    }
}
