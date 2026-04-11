#include "Core/LifePCH.h"
#include "Graphics/Renderer2D.h"

#include "Graphics/Detail/Renderer2DBatching.h"
#include "Graphics/Detail/Renderer2DDetail.h"
#include "Graphics/Detail/Renderer2DResources.h"
#include "Graphics/Detail/Renderer2DSubmission.h"

#include "Assets/TextureAsset.h"
#include "Graphics/Camera.h"
#include "Graphics/RenderCommand.h"
#include "Graphics/Renderer.h"

namespace Life
{
    Renderer2D::Renderer2D(Renderer& renderer)
        : m_Renderer(renderer)
        , m_Impl(CreateScope<Impl>())
    {
        m_Impl->Instances.reserve(static_cast<decltype(m_Impl->Instances)::size_type>(Detail::Renderer2DMaxQuads));
        m_Impl->Stats = {};
    }

    Renderer2D::~Renderer2D() = default;

    void Renderer2D::BeginScene(const Camera& camera)
    {
        Detail::Renderer2DResources resources(*this);
        if (!resources.EnsureResourcesReady())
            return;

        const FramebufferExtent framebufferExtent = m_Renderer.GetFramebufferExtent();
        const Viewport viewport = camera.GetPixelViewport(framebufferExtent);

        RenderCommand::SetViewport(m_Renderer, viewport.X, viewport.Y, viewport.Width, viewport.Height);
        RenderCommand::SetScissor(m_Renderer,
                                  static_cast<int32_t>(viewport.X),
                                  static_cast<int32_t>(viewport.Y),
                                  static_cast<uint32_t>(viewport.Width),
                                  static_cast<uint32_t>(viewport.Height));

        if (camera.GetClearMode() == CameraClearMode::SolidColor)
        {
            const auto& clearColor = camera.GetClearColor();
            RenderCommand::Clear(m_Renderer, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        }

        BeginScene(camera.GetViewProjectionMatrix());
    }

    void Renderer2D::BeginScene(const glm::mat4& viewProjection)
    {
        Detail::Renderer2DResources resources(*this);
        if (!resources.EnsureResourcesReady())
        {
            m_Impl->SceneActive = false;
            return;
        }

        Detail::Renderer2DBatching batching(*this);
        batching.AdvanceActiveBufferVersion();

        ResetStats();

        if (!batching.UpdateSceneConstants(viewProjection))
        {
            m_Impl->SceneActive = false;
            return;
        }

        batching.ResetQueuedDraws();
        m_Impl->SceneActive = true;
    }

    void Renderer2D::EndScene()
    {
        if (!m_Impl->SceneActive)
            return;

        Flush();
        m_Impl->SceneActive = false;
    }

    void Renderer2D::Flush()
    {
        if (!m_Impl->SceneActive || m_Impl->QueuedQuadCount == 0)
            return;

        Detail::Renderer2DSubmission submission(*this);
        submission.SubmitQueuedDraws();
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, m_Impl->WhiteTexture.get(), color);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const TextureResource* texture,
                              const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const TextureResource* texture,
                              const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, texture, color);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                              const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, textureAsset, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                              const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, 0.0f, textureAsset, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, m_Impl->WhiteTexture.get(), color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const TextureResource* texture, const glm::vec4& color)
    {
        if (!m_Impl->SceneActive)
            return;

        Detail::Renderer2DBatching batching(*this);
        batching.PushQuad(position, size, rotationRadians, color, { 0.0f, 0.0f }, { 1.0f, 1.0f }, texture);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const Assets::TextureAsset& textureAsset, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, textureAsset.TryGetTextureResource(), color);
    }

    bool Renderer2D::IsSceneActive() const noexcept
    {
        return m_Impl->SceneActive;
    }

    const Renderer2D::Statistics& Renderer2D::GetStats() const noexcept
    {
        return m_Impl->Stats;
    }

    void Renderer2D::ResetStats() noexcept
    {
        m_Impl->Stats = {};
    }
}
