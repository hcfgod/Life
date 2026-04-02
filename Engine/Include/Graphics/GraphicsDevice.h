#pragma once

#include "Graphics/GraphicsBackend.h"
#include "Core/Memory.h"

#include <cstdint>

namespace nvrhi
{
    class IDevice;
    class ITexture;
    class ICommandList;
}

namespace Life
{
    class Window;

    struct GraphicsDeviceSpecification
    {
        GraphicsBackend Backend = GraphicsBackend::None;
        bool EnableValidation = true;
        bool VSync = true;
    };

    class GraphicsDevice
    {
    public:
        virtual ~GraphicsDevice() = default;

        GraphicsDevice(const GraphicsDevice&) = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;
        GraphicsDevice(GraphicsDevice&&) = delete;
        GraphicsDevice& operator=(GraphicsDevice&&) = delete;

        virtual bool BeginFrame() = 0;
        virtual void Present() = 0;

        virtual nvrhi::ITexture* GetCurrentBackBuffer() = 0;
        virtual nvrhi::IDevice* GetNvrhiDevice() = 0;
        virtual nvrhi::ICommandList* GetCurrentCommandList() = 0;

        virtual uint32_t GetBackBufferWidth() const = 0;
        virtual uint32_t GetBackBufferHeight() const = 0;
        virtual GraphicsBackend GetBackend() const = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

    protected:
        GraphicsDevice() = default;
    };

    Scope<GraphicsDevice> CreateGraphicsDevice(const GraphicsDeviceSpecification& spec, Window& window);
}
