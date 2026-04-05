#pragma once

#include "Graphics/GraphicsTypes.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace Life
{
    enum class VertexAttributeSemantic : uint8_t
    {
        Position = 0,
        Normal,
        Tangent,
        Bitangent,
        Color,
        TexCoord0,
        TexCoord1,
        BoneIndices,
        BoneWeights,
        Custom
    };

    struct VertexAttribute
    {
        std::string Name;
        VertexAttributeSemantic Semantic = VertexAttributeSemantic::Custom;
        TextureFormat Format = TextureFormat::RGBA32_FLOAT;
        uint32_t Offset = 0;
        uint32_t BufferIndex = 0;
    };

    class VertexLayout
    {
    public:
        VertexLayout() = default;
        VertexLayout(std::initializer_list<VertexAttribute> attributes);
        explicit VertexLayout(std::vector<VertexAttribute> attributes);

        const std::vector<VertexAttribute>& GetAttributes() const noexcept { return m_Attributes; }
        uint32_t GetStride() const noexcept { return m_Stride; }
        bool IsEmpty() const noexcept { return m_Attributes.empty(); }

        auto begin() const noexcept { return m_Attributes.begin(); }
        auto end() const noexcept { return m_Attributes.end(); }

        static const VertexLayout& PositionOnly();
        static const VertexLayout& Standard3D();

    private:
        void CalculateOffsetsAndStride();

        std::vector<VertexAttribute> m_Attributes;
        uint32_t m_Stride = 0;
    };

    uint32_t GetFormatSizeBytes(TextureFormat format);
}
