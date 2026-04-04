#pragma once

#include <cstdint>

namespace Life
{
    enum class ShaderStage : uint8_t
    {
        None = 0,
        Vertex,
        Pixel,
        Geometry,
        Hull,
        Domain,
        Compute
    };

    enum class BufferUsage : uint8_t
    {
        Vertex = 0,
        Index,
        Constant,
        Structured
    };

    enum class TextureFormat : uint8_t
    {
        Unknown = 0,
        RGBA8_UNORM,
        RGBA8_SRGB,
        BGRA8_UNORM,
        BGRA8_SRGB,
        RGBA16_FLOAT,
        RGBA32_FLOAT,
        RG16_FLOAT,
        RG32_FLOAT,
        R8_UNORM,
        R16_FLOAT,
        R32_FLOAT,
        Depth16,
        Depth24Stencil8,
        Depth32F
    };

    enum class PrimitiveTopology : uint8_t
    {
        TriangleList = 0,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList
    };

    enum class CullMode : uint8_t
    {
        None = 0,
        Front,
        Back
    };

    enum class FillMode : uint8_t
    {
        Solid = 0,
        Wireframe
    };

    enum class CompareOp : uint8_t
    {
        Never = 0,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class BlendFactor : uint8_t
    {
        Zero = 0,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DstColor,
        InvDstColor,
        DstAlpha,
        InvDstAlpha
    };

    enum class BlendOp : uint8_t
    {
        Add = 0,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum class IndexFormat : uint8_t
    {
        UInt16 = 0,
        UInt32
    };

    struct RasterizerState
    {
        CullMode Cull = CullMode::Back;
        FillMode Fill = FillMode::Solid;
        bool FrontCounterClockwise = false;
        int32_t DepthBias = 0;
        float DepthBiasClamp = 0.0f;
        float SlopeScaledDepthBias = 0.0f;
    };

    struct DepthStencilState
    {
        bool DepthTestEnable = true;
        bool DepthWriteEnable = true;
        CompareOp DepthCompareOp = CompareOp::Less;
        bool StencilEnable = false;
    };

    struct BlendState
    {
        bool BlendEnable = false;
        BlendFactor SrcColorFactor = BlendFactor::One;
        BlendFactor DstColorFactor = BlendFactor::Zero;
        BlendOp ColorOp = BlendOp::Add;
        BlendFactor SrcAlphaFactor = BlendFactor::One;
        BlendFactor DstAlphaFactor = BlendFactor::Zero;
        BlendOp AlphaOp = BlendOp::Add;
    };

    struct Viewport
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;
        float MinDepth = 0.0f;
        float MaxDepth = 1.0f;
    };

    struct ScissorRect
    {
        int32_t X = 0;
        int32_t Y = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };
}
