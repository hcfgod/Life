#pragma once

namespace Life
{
    enum class GraphicsBackend
    {
        None = 0,
        Vulkan,
        D3D12
    };

    GraphicsBackend GetPreferredGraphicsBackend();
}
