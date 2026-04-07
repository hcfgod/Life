#pragma once

#include "Core/Memory.h"

#include <cstdint>

namespace Life
{
    class Camera;
    class ImGuiSystem;
    class Renderer;
    class Renderer2D;
    class TextureResource;

    class SceneViewport
    {
    public:
        SceneViewport(Renderer& renderer, Renderer2D& renderer2D, ImGuiSystem& imguiSystem);
        ~SceneViewport() noexcept;

        SceneViewport(const SceneViewport&) = delete;
        SceneViewport& operator=(const SceneViewport&) = delete;
        SceneViewport(SceneViewport&&) = delete;
        SceneViewport& operator=(SceneViewport&&) = delete;

        bool Resize(uint32_t width, uint32_t height);
        bool BeginRender2D(const Camera& camera);
        void EndRender2D() noexcept;
        bool Draw(float width, float height);
        void Reset() noexcept;

        Renderer2D& GetRenderer2D() noexcept { return m_Renderer2D; }
        const Renderer2D& GetRenderer2D() const noexcept { return m_Renderer2D; }
        bool IsReady() const noexcept;
        uint32_t GetWidth() const noexcept { return m_Width; }
        uint32_t GetHeight() const noexcept { return m_Height; }
        TextureResource* GetColorTarget() noexcept;
        const TextureResource* GetColorTarget() const noexcept;

    private:
        bool BeginRender();
        void EndRender() noexcept;

        Renderer& m_Renderer;
        Renderer2D& m_Renderer2D;
        ImGuiSystem& m_ImGuiSystem;
        Scope<TextureResource> m_ColorTarget;
        TextureResource* m_PreviousRenderTarget = nullptr;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        bool m_RenderActive = false;
    };
}
