#include "Core/LifePCH.h"
#include "Graphics/SceneSurface.h"

#include "Core/Log.h"
#include "Graphics/Camera.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ImGuiSystem.h"
#include "Graphics/Renderer.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/TextureResource.h"

#include <nvrhi/nvrhi.h>

#include <algorithm>

namespace Life
{
    namespace
    {
        constexpr uint32_t SceneSurfaceSupersampleFactor = 1u;
        constexpr uint32_t SceneSurfaceSampleCount = 4u;
    }

    SceneSurface::SceneSurface(Renderer& renderer, Renderer2D& renderer2D, ImGuiSystem& imguiSystem)
        : m_Renderer(renderer)
        , m_Renderer2D(renderer2D)
        , m_ImGuiSystem(imguiSystem)
    {
    }

    SceneSurface::~SceneSurface() noexcept
    {
        Reset();
    }

    bool SceneSurface::Resize(uint32_t width, uint32_t height)
    {
        width = std::max(width, 1u);
        height = std::max(height, 1u);
        const uint32_t renderWidth = std::max(width * SceneSurfaceSupersampleFactor, 1u);
        const uint32_t renderHeight = std::max(height * SceneSurfaceSupersampleFactor, 1u);
        if (m_ColorTarget && m_ColorTarget->IsValid() && m_Width == renderWidth && m_Height == renderHeight)
            return true;

        Reset();

        TextureDescription textureDescription;
        textureDescription.DebugName = "SceneSurfaceColorTarget";
        textureDescription.Width = renderWidth;
        textureDescription.Height = renderHeight;
        textureDescription.Format = TextureFormat::BGRA8_UNORM;
        textureDescription.IsRenderTarget = true;
        textureDescription.Sampler.MinFilter = TextureFilterMode::Nearest;
        textureDescription.Sampler.MagFilter = TextureFilterMode::Nearest;
        textureDescription.Sampler.WrapU = TextureWrapMode::ClampToEdge;
        textureDescription.Sampler.WrapV = TextureWrapMode::ClampToEdge;

        m_ColorTarget = TextureResource::Create2D(m_Renderer.GetGraphicsDevice(), textureDescription);
        if (!m_ColorTarget)
        {
            LOG_CORE_ERROR("SceneSurface failed to create a render target at {}x{}.", width, height);
            return false;
        }

        if (SceneSurfaceSampleCount > 1u)
        {
            TextureDescription multisampleDescription = textureDescription;
            multisampleDescription.DebugName = "SceneSurfaceMultisampleColorTarget";
            multisampleDescription.SampleCount = SceneSurfaceSampleCount;
            m_MultisampleColorTarget = TextureResource::Create2D(m_Renderer.GetGraphicsDevice(), multisampleDescription);
            if (!m_MultisampleColorTarget)
            {
                LOG_CORE_WARN("SceneSurface could not create a {}x multisample color target at {}x{}; falling back to single-sample rendering.",
                              SceneSurfaceSampleCount,
                              width,
                              height);
            }
        }

        TextureDescription depthDescription;
        depthDescription.DebugName = "SceneSurfaceDepthTarget";
        depthDescription.Width = renderWidth;
        depthDescription.Height = renderHeight;
        depthDescription.Format = TextureFormat::Depth32F;
        depthDescription.IsDepthStencil = true;
        depthDescription.SampleCount = m_MultisampleColorTarget ? SceneSurfaceSampleCount : 1u;

        m_DepthTarget = TextureResource::Create2D(m_Renderer.GetGraphicsDevice(), depthDescription);
        if (!m_DepthTarget)
        {
            Reset();
            LOG_CORE_ERROR("SceneSurface failed to create a depth target at {}x{}.", width, height);
            return false;
        }

        m_Width = renderWidth;
        m_Height = renderHeight;
        return true;
    }

    bool SceneSurface::BeginScene2D(const Camera& camera)
    {
        if (!IsReady())
            return false;

        if (!BeginSurfaceRender())
            return false;

        const glm::vec4& clearColor = camera.GetClearColor();
        m_Renderer.Clear(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        m_Renderer2D.BeginScene(camera);
        if (!m_Renderer2D.IsSceneActive())
        {
            EndSurfaceRender();
            return false;
        }

        return true;
    }

    void SceneSurface::EndScene2D() noexcept
    {
        if (!m_RenderActive)
            return;

        m_Renderer2D.EndScene();
        EndSurfaceRender();
    }

    bool SceneSurface::Present(float width, float height)
    {
        if (!IsReady() || m_RenderActive)
            return false;

        return m_ImGuiSystem.DrawImage(*m_ColorTarget, width, height, ImGuiTextureSampling::Nearest);
    }

    void SceneSurface::Reset() noexcept
    {
        if (m_RenderActive)
            EndScene2D();

        if (m_ColorTarget)
            m_ImGuiSystem.ReleaseTextureHandle(*m_ColorTarget);

        m_ColorTarget.reset();
        m_MultisampleColorTarget.reset();
        m_DepthTarget.reset();
        m_Width = 0;
        m_Height = 0;
        m_RenderActive = false;
    }

    bool SceneSurface::IsReady() const noexcept
    {
        return m_ColorTarget != nullptr &&
               m_ColorTarget->IsValid() &&
               m_DepthTarget != nullptr &&
               m_DepthTarget->IsValid();
    }

    bool SceneSurface::BeginSurfaceRender()
    {
        TextureResource* activeColorTarget = GetActiveColorTarget();
        if (!activeColorTarget || m_RenderActive)
            return false;

        if (!m_Renderer.PushRenderTarget(*activeColorTarget, m_DepthTarget.get()))
            return false;

        m_Renderer.ClearDepth();
        m_RenderActive = true;
        return true;
    }

    void SceneSurface::EndSurfaceRender() noexcept
    {
        if (!m_RenderActive)
            return;

        m_Renderer.PopRenderTarget();
        ResolveMultisampleColorTarget();
        m_RenderActive = false;
    }

    TextureResource* SceneSurface::GetActiveColorTarget() noexcept
    {
        return m_MultisampleColorTarget ? m_MultisampleColorTarget.get() : m_ColorTarget.get();
    }

    const TextureResource* SceneSurface::GetActiveColorTarget() const noexcept
    {
        return m_MultisampleColorTarget ? m_MultisampleColorTarget.get() : m_ColorTarget.get();
    }

    void SceneSurface::ResolveMultisampleColorTarget() noexcept
    {
        if (!m_MultisampleColorTarget || !m_ColorTarget)
            return;

        nvrhi::ICommandList* commandList = m_Renderer.GetGraphicsDevice().GetCurrentCommandList();
        nvrhi::ITexture* sourceTexture = m_MultisampleColorTarget->GetNativeHandle();
        nvrhi::ITexture* destinationTexture = m_ColorTarget->GetNativeHandle();
        if (commandList == nullptr || sourceTexture == nullptr || destinationTexture == nullptr)
            return;

        commandList->setTextureState(sourceTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ResolveSource);
        commandList->setTextureState(destinationTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ResolveDest);
        commandList->commitBarriers();
        commandList->resolveTexture(destinationTexture, nvrhi::AllSubresources, sourceTexture, nvrhi::AllSubresources);
        commandList->setTextureState(destinationTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();
    }
}
