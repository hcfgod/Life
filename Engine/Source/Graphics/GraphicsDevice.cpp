#include "Core/LifePCH.h"
#include "Graphics/GraphicsDevice.h"
#include "Core/Log.h"
#include "Core/Error.h"

#ifdef LIFE_GRAPHICS_VULKAN
#include "Graphics/Vulkan/VulkanGraphicsDevice.h"
#endif

namespace Life
{
    GraphicsBackend GetPreferredGraphicsBackend()
    {
#if defined(LIFE_GRAPHICS_VULKAN)
        return GraphicsBackend::Vulkan;
#elif defined(LIFE_GRAPHICS_D3D12) && defined(_WIN32)
        return GraphicsBackend::D3D12;
#else
        return GraphicsBackend::None;
#endif
    }

    Scope<GraphicsDevice> CreateGraphicsDevice(const GraphicsDeviceSpecification& spec, Window& window)
    {
        GraphicsBackend backend = spec.Backend;
        if (backend == GraphicsBackend::None)
            backend = GetPreferredGraphicsBackend();

        switch (backend)
        {
#ifdef LIFE_GRAPHICS_VULKAN
        case GraphicsBackend::Vulkan:
            LOG_CORE_INFO("Creating Vulkan graphics device...");
            return CreateScope<VulkanGraphicsDevice>(spec, window);
#endif
        default:
            throw Error(
                ErrorCode::GraphicsError,
                "No supported graphics backend available.",
                std::source_location::current(),
                ErrorSeverity::Critical);
        }
    }
}
