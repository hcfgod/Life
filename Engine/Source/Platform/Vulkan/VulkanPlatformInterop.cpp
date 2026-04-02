#include "Core/LifePCH.h"
#include "Platform/Vulkan/VulkanPlatformInterop.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace Life::Platform::VulkanInterop
{
    namespace
    {
        SDL_Window* GetSDLWindow(const Window& window)
        {
            return static_cast<SDL_Window*>(window.GetNativeHandle());
        }
    }

    bool WindowSupportsVulkan(const Window& window)
    {
        SDL_Window* sdlWindow = GetSDLWindow(window);
        if (sdlWindow == nullptr)
            return false;

        return (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_VULKAN) != 0;
    }

    std::vector<const char*> GetRequiredInstanceExtensions(const Window& window)
    {
        if (!WindowSupportsVulkan(window))
            return {};

        Uint32 extensionCount = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (extensions == nullptr)
            return {};

        return std::vector<const char*>(extensions, extensions + extensionCount);
    }

    VkSurfaceKHR CreateSurface(const Window& window, VkInstance instance)
    {
        SDL_Window* sdlWindow = GetSDLWindow(window);
        if (sdlWindow == nullptr || instance == VK_NULL_HANDLE)
            return VK_NULL_HANDLE;

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(sdlWindow, instance, nullptr, &surface))
            return VK_NULL_HANDLE;

        return surface;
    }
}
