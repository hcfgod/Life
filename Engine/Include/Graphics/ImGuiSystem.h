#pragma once

#include "Core/Memory.h"
#include "Graphics/GraphicsBackend.h"
#include "Graphics/TextureResource.h"

#include <cstdint>

union SDL_Event;

namespace Life
{
    class Event;
    class GraphicsDevice;
    class TextureResource;
    class Window;

    enum class ImGuiTextureSampling : uint8_t
    {
        Linear = 0,
        Nearest = 1
    };

    class ImGuiSystem
    {
    public:
        ImGuiSystem(Window& window, GraphicsDevice* graphicsDevice);
        ~ImGuiSystem() noexcept;

        ImGuiSystem(const ImGuiSystem&) = delete;
        ImGuiSystem& operator=(const ImGuiSystem&) = delete;
        ImGuiSystem(ImGuiSystem&&) = delete;
        ImGuiSystem& operator=(ImGuiSystem&&) = delete;

        void Initialize();
        void Shutdown() noexcept;
        void BeginFrame();
        void Render();
        void OnSdlEvent(const SDL_Event& event);
        void CaptureEvent(Event& event);
        bool DrawImage(TextureResource& texture, float width, float height, ImGuiTextureSampling sampling = ImGuiTextureSampling::Linear);
        void ReleaseTextureHandle(TextureResource& texture) noexcept;

        bool IsInitialized() const noexcept { return m_Initialized; }
        bool IsAvailable() const noexcept { return m_Available; }
        bool IsFrameActive() const noexcept { return m_FrameActive; }
        GraphicsBackend GetBackend() const noexcept { return m_Backend; }
        bool WantsKeyboardCapture() const noexcept;
        bool WantsMouseCapture() const noexcept;

    private:
        Window& m_Window;
        GraphicsDevice* m_GraphicsDevice = nullptr;
        GraphicsBackend m_Backend = GraphicsBackend::None;
        bool m_Initialized = false;
        bool m_Available = false;
        bool m_FrameActive = false;

        struct Impl;
        Scope<Impl> m_Impl;
    };
}
