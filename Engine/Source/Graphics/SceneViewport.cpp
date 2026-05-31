#include "Core/LifePCH.h"
#include "Graphics/SceneViewport.h"

#include "Core/Log.h"
#include "Graphics/Camera.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ImGuiSystem.h"
#include "Graphics/Renderer.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/TextureResource.h"

#include <algorithm>

namespace Life
{
    SceneViewport::SceneViewport(Renderer& renderer, Renderer2D& renderer2D, ImGuiSystem& imguiSystem)
        : m_Renderer(renderer)
        , m_Renderer2D(renderer2D)
        , m_ImGuiSystem(imguiSystem)
    {
    }

    SceneViewport::~SceneViewport() noexcept
    {
        Reset();
    }

    bool SceneViewport::Resize(uint32_t width, uint32_t height)
    {
        width = std::max(width, 1u);
        height = std::max(height, 1u);
        if (m_ColorTarget && m_Width == width && m_Height == height)
            return true;

        Reset();

        TextureDescription textureDescription;
        textureDescription.DebugName = "SceneViewportColorTarget";
        textureDescription.Width = width;
        textureDescription.Height = height;
        textureDescription.Format = TextureFormat::BGRA8_UNORM;
        textureDescription.IsRenderTarget = true;

        m_ColorTarget = TextureResource::Create2D(m_Renderer.GetGraphicsDevice(), textureDescription);
        if (!m_ColorTarget)
        {
            LOG_CORE_ERROR("SceneViewport failed to create a render target at {}x{}.", width, height);
            return false;
        }

        m_Width = width;
        m_Height = height;
        return true;
    }

    bool SceneViewport::BeginRender2D(const Camera& camera)
    {
        if (!m_ColorTarget)
            return false;

        if (!BeginRender())
            return false;

        m_Renderer2D.BeginScene(camera);
        return true;
    }

    void SceneViewport::EndRender2D() noexcept
    {
        if (!m_RenderActive)
            return;

        m_Renderer2D.EndScene();
        EndRender();
    }

    bool SceneViewport::Draw(float width, float height)
    {
        if (!m_ColorTarget)
            return false;

        return m_ImGuiSystem.DrawImage(*m_ColorTarget, width, height, ImGuiTextureSampling::Nearest);
    }

    void SceneViewport::Reset() noexcept
    {
        if (m_RenderActive)
            EndRender2D();

        if (m_ColorTarget)
            m_ImGuiSystem.ReleaseTextureHandle(*m_ColorTarget);

        m_ColorTarget.reset();
        m_Width = 0;
        m_Height = 0;
        m_RenderActive = false;
    }

    bool SceneViewport::IsReady() const noexcept
    {
        return m_ColorTarget != nullptr;
    }

    TextureResource* SceneViewport::GetColorTarget() noexcept
    {
        return m_ColorTarget.get();
    }

    const TextureResource* SceneViewport::GetColorTarget() const noexcept
    {
        return m_ColorTarget.get();
    }

    bool SceneViewport::BeginRender()
    {
        if (!m_ColorTarget || m_RenderActive)
            return false;

        if (!m_Renderer.PushRenderTarget(*m_ColorTarget))
            return false;

        m_RenderActive = true;
        return true;
    }

    void SceneViewport::EndRender() noexcept
    {
        if (!m_RenderActive)
            return;

        m_Renderer.PopRenderTarget();
        m_RenderActive = false;
    }
}
