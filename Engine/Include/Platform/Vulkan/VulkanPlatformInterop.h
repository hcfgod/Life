#pragma once

#include "Core/Window.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace Life::Platform::VulkanInterop
{
    bool WindowSupportsVulkan(const Window& window);
    std::vector<const char*> GetRequiredInstanceExtensions(const Window& window);
    VkSurfaceKHR CreateSurface(const Window& window, VkInstance instance);
}
