#include "Core/LifePCH.h"
#include "Graphics/Detail/ImGuiRendererBackend.h"

#include "Graphics/Detail/ImGuiVulkanRendererBackend.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/Vulkan/VulkanGraphicsDevice.h"

namespace Life::Detail
{
#if LIFE_HAS_IMGUI
    Scope<ImGuiRendererBackend> CreateImGuiRendererBackend(GraphicsDevice& graphicsDevice)
    {
        if (graphicsDevice.GetBackend() == GraphicsBackend::Vulkan)
        {
#if LIFE_HAS_IMGUI_VULKAN
            if (auto* vulkanGraphicsDevice = dynamic_cast<VulkanGraphicsDevice*>(&graphicsDevice))
                return CreateScope<ImGuiVulkanRendererBackend>(*vulkanGraphicsDevice);
#endif
        }

        return nullptr;
    }
#else
    Scope<ImGuiRendererBackend> CreateImGuiRendererBackend(GraphicsDevice& graphicsDevice)
    {
        (void)graphicsDevice;
        return nullptr;
    }
#endif
}
