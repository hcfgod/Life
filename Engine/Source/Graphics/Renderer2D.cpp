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
#include <cmath>

namespace Life
{
    namespace
    {
        Viewport ResolveIntegralViewport(const Viewport& viewport, const FramebufferExtent& framebufferExtent) noexcept
        {
            const float maxWidth = static_cast<float>(framebufferExtent.Width);
            const float maxHeight = static_cast<float>(framebufferExtent.Height);
            const float left = std::clamp(std::floor(viewport.X), 0.0f, maxWidth);
            const float top = std::clamp(std::floor(viewport.Y), 0.0f, maxHeight);
            const float right = std::clamp(std::ceil(viewport.X + viewport.Width), left, maxWidth);
            const float bottom = std::clamp(std::ceil(viewport.Y + viewport.Height), top, maxHeight);

            Viewport resolvedViewport = viewport;
            resolvedViewport.X = left;
            resolvedViewport.Y = top;
            resolvedViewport.Width = std::max(right - left, 0.0f);
            resolvedViewport.Height = std::max(bottom - top, 0.0f);
            return resolvedViewport;
        }

        bool IsOpaqueQuadSubmission(const TextureResource* texture, const TextureResource* whiteTexture, const glm::vec4& color) noexcept
        {
            constexpr float OpaqueAlphaThreshold = 1.0f - (0.5f / 255.0f);
            return texture == whiteTexture && color.a >= OpaqueAlphaThreshold;
        }

        std::pair<glm::vec2, glm::vec2> ResolveFullTextureUvBounds(const TextureResource* texture)
        {
            if (texture == nullptr)
                return { glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f) };

            const float inverseWidth = 1.0f / std::max(static_cast<float>(texture->GetWidth()), 1.0f);
            const float inverseHeight = 1.0f / std::max(static_cast<float>(texture->GetHeight()), 1.0f);
            const glm::vec2 halfTexel(0.5f * inverseWidth, 0.5f * inverseHeight);
            return { halfTexel, glm::vec2(1.0f, 1.0f) - halfTexel };
        }
    }

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
        const Viewport viewport = ResolveIntegralViewport(camera.GetPixelViewport(framebufferExtent), framebufferExtent);

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
        m_Impl->SubmittedQuadCount = 0;
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

    void Renderer2D::SetDepthTestingEnabled(bool enabled) noexcept
    {
        m_Impl->DepthTestingEnabled = enabled;
    }

    bool Renderer2D::IsDepthTestingEnabled() const noexcept
    {
        return m_Impl->DepthTestingEnabled;
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawQuad(position,
                 glm::vec3(size.x, 0.0f, 0.0f),
                 glm::vec3(0.0f, size.y, 0.0f),
                 m_Impl->WhiteTexture.get(),
                 color);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const TextureResource* texture,
                              const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, texture, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const TextureResource* texture,
                              const glm::vec4& color)
    {
        DrawQuad(position,
                 glm::vec3(size.x, 0.0f, 0.0f),
                 glm::vec3(0.0f, size.y, 0.0f),
                 texture,
                 color);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                              const glm::vec4& color)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, textureAsset, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                              const glm::vec4& color)
    {
        DrawQuad(position,
                 glm::vec3(size.x, 0.0f, 0.0f),
                 glm::vec3(0.0f, size.y, 0.0f),
                 textureAsset,
                 color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, m_Impl->WhiteTexture.get(), color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const TextureResource* texture, const glm::vec4& color)
    {
        const float sineRotation = std::sin(rotationRadians);
        const float cosineRotation = std::cos(rotationRadians);
        const glm::vec3 xAxis{ cosineRotation * size.x, sineRotation * size.x, 0.0f };
        const glm::vec3 yAxis{ -sineRotation * size.y, cosineRotation * size.y, 0.0f };
        DrawQuad(position, xAxis, yAxis, texture, color);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                                     const Assets::TextureAsset& textureAsset, const glm::vec4& color)
    {
        DrawRotatedQuad(position, size, rotationRadians, textureAsset.TryGetTextureResource(), color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& center, const glm::vec3& xAxis, const glm::vec3& yAxis,
                              const glm::vec4& color)
    {
        DrawQuad(center, xAxis, yAxis, m_Impl->WhiteTexture.get(), color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& center, const glm::vec3& xAxis, const glm::vec3& yAxis,
                              const TextureResource* texture, const glm::vec4& color)
    {
        if (!m_Impl->SceneActive)
            return;

        const auto [uvMin, uvMax] = ResolveFullTextureUvBounds(texture);
        Detail::Renderer2DBatching batching(*this);
        const Detail::Renderer2DBatching::QuadSubmission quad = {
            center,
            xAxis,
            yAxis,
            color,
            uvMin,
            uvMax,
            texture,
            IsOpaqueQuadSubmission(texture, m_Impl->WhiteTexture.get(), color)
        };
        batching.PushQuad(quad);
    }

    void Renderer2D::DrawQuad(const glm::vec3& center, const glm::vec3& xAxis, const glm::vec3& yAxis,
                              const Assets::TextureAsset& textureAsset, const glm::vec4& color)
    {
        DrawQuad(center, xAxis, yAxis, textureAsset.TryGetTextureResource(), color);
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
