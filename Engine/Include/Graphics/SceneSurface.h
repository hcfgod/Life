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

    class SceneSurface
    {
    public:
        SceneSurface(Renderer& renderer, Renderer2D& renderer2D, ImGuiSystem& imguiSystem);
        ~SceneSurface() noexcept;

        SceneSurface(const SceneSurface&) = delete;
        SceneSurface& operator=(const SceneSurface&) = delete;
        SceneSurface(SceneSurface&&) = delete;
        SceneSurface& operator=(SceneSurface&&) = delete;

        bool Resize(uint32_t width, uint32_t height);
        bool BeginScene2D(const Camera& camera);
        void EndScene2D() noexcept;
        bool Present(float width, float height);
        void Reset() noexcept;

        Renderer2D& GetRenderer2D() noexcept { return m_Renderer2D; }
        const Renderer2D& GetRenderer2D() const noexcept { return m_Renderer2D; }
        bool IsReady() const noexcept;
        uint32_t GetWidth() const noexcept { return m_Width; }
        uint32_t GetHeight() const noexcept { return m_Height; }

    private:
        bool BeginSurfaceRender();
        void EndSurfaceRender() noexcept;

        Renderer& m_Renderer;
        Renderer2D& m_Renderer2D;
        ImGuiSystem& m_ImGuiSystem;
        Scope<TextureResource> m_ColorTarget;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        bool m_RenderActive = false;
    };
}
