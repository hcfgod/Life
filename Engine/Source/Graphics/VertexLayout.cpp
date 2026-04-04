#include "Core/LifePCH.h"
#include "Graphics/VertexLayout.h"

namespace Life
{
    VertexAttribute::VertexAttribute(std::string name, VertexAttributeSemantic semantic, TextureFormat format,
                                     uint32_t offset, uint32_t bufferIndex)
        : Name(std::move(name))
        , Semantic(semantic)
        , Format(format)
        , Offset(offset)
        , BufferIndex(bufferIndex)
    {
    }

    uint32_t GetFormatSizeBytes(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::R8_UNORM:         return 1;
        case TextureFormat::Depth16:          return 2;
        case TextureFormat::R16_FLOAT:        return 2;
        case TextureFormat::R32_FLOAT:        return 4;
        case TextureFormat::RG16_FLOAT:       return 4;
        case TextureFormat::Depth24Stencil8:  return 4;
        case TextureFormat::RGBA8_UNORM:      return 4;
        case TextureFormat::RGBA8_SRGB:       return 4;
        case TextureFormat::BGRA8_UNORM:      return 4;
        case TextureFormat::BGRA8_SRGB:       return 4;
        case TextureFormat::Depth32F:         return 4;
        case TextureFormat::RG32_FLOAT:       return 8;
        case TextureFormat::RGBA16_FLOAT:     return 8;
        case TextureFormat::RGBA32_FLOAT:     return 16;
        default:                              return 0;
        }
    }

    VertexLayout::VertexLayout(std::initializer_list<VertexAttribute> attributes)
        : m_Attributes(attributes)
    {
        CalculateOffsetsAndStride();
    }

    VertexLayout::VertexLayout(std::vector<VertexAttribute> attributes)
        : m_Attributes(std::move(attributes))
    {
        CalculateOffsetsAndStride();
    }

    void VertexLayout::CalculateOffsetsAndStride()
    {
        uint32_t offset = 0;
        for (auto& attribute : m_Attributes)
        {
            attribute.Offset = offset;
            offset += GetFormatSizeBytes(attribute.Format);
        }
        m_Stride = offset;
    }

    const VertexLayout& VertexLayout::PositionOnly()
    {
        static VertexLayout layout =
        {
            { "POSITION", VertexAttributeSemantic::Position, TextureFormat::RGBA32_FLOAT }
        };
        return layout;
    }

    const VertexLayout& VertexLayout::Standard3D()
    {
        static VertexLayout layout =
        {
            { "POSITION",  VertexAttributeSemantic::Position,  TextureFormat::RGBA32_FLOAT },
            { "NORMAL",    VertexAttributeSemantic::Normal,    TextureFormat::RGBA32_FLOAT },
            { "TEXCOORD",  VertexAttributeSemantic::TexCoord0, TextureFormat::RG32_FLOAT   },
            { "TANGENT",   VertexAttributeSemantic::Tangent,   TextureFormat::RGBA32_FLOAT }
        };
        return layout;
    }
}
