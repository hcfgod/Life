#include "Core/LifePCH.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsDevice.h"

#include <nvrhi/nvrhi.h>

namespace Life
{
    struct GraphicsBuffer::Impl
    {
        nvrhi::BufferHandle Handle;
    };

    GraphicsBuffer::~GraphicsBuffer() = default;

    GraphicsBuffer::GraphicsBuffer(GraphicsBuffer&& other) noexcept
        : m_Impl(std::move(other.m_Impl))
        , m_Description(std::move(other.m_Description))
    {
    }

    GraphicsBuffer& GraphicsBuffer::operator=(GraphicsBuffer&& other) noexcept
    {
        if (this != &other)
        {
            m_Impl = std::move(other.m_Impl);
            m_Description = std::move(other.m_Description);
        }
        return *this;
    }

    bool GraphicsBuffer::IsValid() const noexcept
    {
        return m_Impl && m_Impl->Handle;
    }

    nvrhi::IBuffer* GraphicsBuffer::GetNativeHandle() const
    {
        return m_Impl ? m_Impl->Handle.Get() : nullptr;
    }

    Scope<GraphicsBuffer> GraphicsBuffer::CreateVertex(GraphicsDevice& device, const void* data,
                                                        uint32_t sizeInBytes, uint32_t stride,
                                                        const std::string& debugName)
    {
        nvrhi::IDevice* nvrhiDevice = device.GetNvrhiDevice();
        if (!nvrhiDevice)
            return nullptr;

        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = sizeInBytes;
        bufferDesc.isVertexBuffer = true;
        bufferDesc.debugName = debugName.c_str();
        bufferDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
        bufferDesc.keepInitialState = true;

        nvrhi::BufferHandle handle = nvrhiDevice->createBuffer(bufferDesc);
        if (!handle)
            return nullptr;

        if (data)
        {
            nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();
            commandList->open();
            commandList->writeBuffer(handle, data, sizeInBytes);
            commandList->close();
            nvrhiDevice->executeCommandList(commandList);
        }

        Scope<GraphicsBuffer> buffer(new GraphicsBuffer());
        buffer->m_Impl = CreateScope<Impl>();
        buffer->m_Impl->Handle = handle;
        buffer->m_Description.DebugName = debugName;
        buffer->m_Description.Usage = BufferUsage::Vertex;
        buffer->m_Description.SizeInBytes = sizeInBytes;
        buffer->m_Description.Stride = stride;
        return buffer;
    }

    Scope<GraphicsBuffer> GraphicsBuffer::CreateIndex(GraphicsDevice& device, const void* data,
                                                       uint32_t sizeInBytes, IndexFormat format,
                                                       const std::string& debugName)
    {
        nvrhi::IDevice* nvrhiDevice = device.GetNvrhiDevice();
        if (!nvrhiDevice)
            return nullptr;

        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = sizeInBytes;
        bufferDesc.isIndexBuffer = true;
        bufferDesc.debugName = debugName.c_str();
        bufferDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
        bufferDesc.keepInitialState = true;

        nvrhi::BufferHandle handle = nvrhiDevice->createBuffer(bufferDesc);
        if (!handle)
            return nullptr;

        if (data)
        {
            nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();
            commandList->open();
            commandList->writeBuffer(handle, data, sizeInBytes);
            commandList->close();
            nvrhiDevice->executeCommandList(commandList);
        }

        uint32_t stride = (format == IndexFormat::UInt32) ? 4 : 2;

        Scope<GraphicsBuffer> buffer(new GraphicsBuffer());
        buffer->m_Impl = CreateScope<Impl>();
        buffer->m_Impl->Handle = handle;
        buffer->m_Description.DebugName = debugName;
        buffer->m_Description.Usage = BufferUsage::Index;
        buffer->m_Description.SizeInBytes = sizeInBytes;
        buffer->m_Description.Stride = stride;
        return buffer;
    }

    Scope<GraphicsBuffer> GraphicsBuffer::CreateConstant(GraphicsDevice& device, uint32_t sizeInBytes,
                                                          const std::string& debugName)
    {
        nvrhi::IDevice* nvrhiDevice = device.GetNvrhiDevice();
        if (!nvrhiDevice)
            return nullptr;

        nvrhi::BufferDesc bufferDesc;
        bufferDesc.byteSize = sizeInBytes;
        bufferDesc.isConstantBuffer = true;
        bufferDesc.debugName = debugName.c_str();
        bufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        bufferDesc.keepInitialState = true;
        bufferDesc.cpuAccess = nvrhi::CpuAccessMode::Write;

        nvrhi::BufferHandle handle = nvrhiDevice->createBuffer(bufferDesc);
        if (!handle)
            return nullptr;

        Scope<GraphicsBuffer> buffer(new GraphicsBuffer());
        buffer->m_Impl = CreateScope<Impl>();
        buffer->m_Impl->Handle = handle;
        buffer->m_Description.DebugName = debugName;
        buffer->m_Description.Usage = BufferUsage::Constant;
        buffer->m_Description.SizeInBytes = sizeInBytes;
        buffer->m_Description.Stride = 0;
        buffer->m_Description.CPUAccess = true;
        return buffer;
    }
}
