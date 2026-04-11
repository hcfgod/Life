#pragma once

#include "Core/Memory.h"

#include <SDL3/SDL.h>

namespace Life
{
    class GraphicsDevice;
    class TextureResource;
}

#if __has_include(<imgui.h>) && __has_include(<backends/imgui_impl_sdl3.h>)
#define LIFE_HAS_IMGUI 1
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#else
#define LIFE_HAS_IMGUI 0
#endif

#if LIFE_HAS_IMGUI && __has_include(<backends/imgui_impl_vulkan.h>)
#define LIFE_HAS_IMGUI_VULKAN 1
#else
#define LIFE_HAS_IMGUI_VULKAN 0
#endif

namespace Life::Detail
{
#if LIFE_HAS_IMGUI
    class ImGuiRendererBackend
    {
    public:
        virtual ~ImGuiRendererBackend() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() noexcept = 0;
        virtual void NewFrame() = 0;
        virtual void RenderDrawData(ImDrawData* drawData) = 0;
        virtual void* GetTextureHandle(TextureResource& texture) = 0;
        virtual void ReleaseTextureHandle(TextureResource& texture) noexcept = 0;
    };
#else
    class ImGuiRendererBackend;
#endif

    Scope<ImGuiRendererBackend> CreateImGuiRendererBackend(GraphicsDevice& graphicsDevice);
}
