#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Memory.h"

#include <cstdint>
#include <string>

namespace nvrhi
{
    class IBuffer;
}

namespace Life
{
    class GraphicsDevice;

    struct GraphicsBufferDescription
    {
        std::string DebugName;
        BufferUsage Usage = BufferUsage::Vertex;
        uint32_t SizeInBytes = 0;
        uint32_t Stride = 0;
        bool CPUAccess = false;
    };

    class GraphicsBuffer
    {
    public:
        ~GraphicsBuffer();

        GraphicsBuffer(const GraphicsBuffer&) = delete;
        GraphicsBuffer& operator=(const GraphicsBuffer&) = delete;
        GraphicsBuffer(GraphicsBuffer&&) noexcept;
        GraphicsBuffer& operator=(GraphicsBuffer&&) noexcept;

        const GraphicsBufferDescription& GetDescription() const noexcept { return m_Description; }
        uint32_t GetSizeInBytes() const noexcept { return m_Description.SizeInBytes; }
        uint32_t GetStride() const noexcept { return m_Description.Stride; }
        BufferUsage GetUsage() const noexcept { return m_Description.Usage; }
        bool IsValid() const noexcept;

        nvrhi::IBuffer* GetNativeHandle() const;

        static Scope<GraphicsBuffer> CreateVertex(GraphicsDevice& device, const void* data,
                                                   uint32_t sizeInBytes, uint32_t stride,
                                                   const std::string& debugName = "VertexBuffer");

        static Scope<GraphicsBuffer> CreateIndex(GraphicsDevice& device, const void* data,
                                                  uint32_t sizeInBytes, IndexFormat format,
                                                  const std::string& debugName = "IndexBuffer");

        static Scope<GraphicsBuffer> CreateConstant(GraphicsDevice& device, uint32_t sizeInBytes,
                                                     const std::string& debugName = "ConstantBuffer");

    private:
        friend class Renderer;

        GraphicsBuffer() = default;

        struct Impl;
        Scope<Impl> m_Impl;
        GraphicsBufferDescription m_Description;
    };
}
