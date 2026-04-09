#pragma once

#include "Core/Memory.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Life
{
    namespace Assets
    {
        class TextureAsset;
    }

    class Camera;
    class GraphicsBuffer;
    class GraphicsPipeline;
    class Renderer;
    class TextureResource;

    class Renderer2D
    {
    public:
        struct Statistics
        {
            uint32_t DrawCalls = 0;
            uint32_t QuadCount = 0;
        };

        explicit Renderer2D(Renderer& renderer);
        ~Renderer2D();

        Renderer2D(const Renderer2D&) = delete;
        Renderer2D& operator=(const Renderer2D&) = delete;

        void BeginScene(const Camera& camera);
        void BeginScene(const glm::mat4& viewProjection);
        void EndScene();
        void Flush();

        void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
        void DrawQuad(const glm::vec2& position, const glm::vec2& size, const TextureResource* texture,
                      const glm::vec4& color = glm::vec4(1.0f));
        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const TextureResource* texture,
                      const glm::vec4& color = glm::vec4(1.0f));
        void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                      const glm::vec4& color = glm::vec4(1.0f));
        void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Assets::TextureAsset& textureAsset,
                      const glm::vec4& color = glm::vec4(1.0f));
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                             const glm::vec4& color);
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                             const TextureResource* texture, const glm::vec4& color = glm::vec4(1.0f));
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                             const Assets::TextureAsset& textureAsset, const glm::vec4& color = glm::vec4(1.0f));

        bool IsSceneActive() const noexcept;
        const Statistics& GetStats() const noexcept;
        void ResetStats() noexcept;

    private:
        bool EnsureResourcesReady();
        void InvalidateResources() noexcept;
        bool UpdateSceneConstants(const glm::mat4& viewProjection);
        void ResetQueuedDraws() noexcept;
        void SubmitQueuedDraws();
        void PushQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians, const glm::vec4& color,
                      const glm::vec2& uvMin, const glm::vec2& uvMax, const TextureResource* texture);

        Renderer& m_Renderer;

        struct Impl;
        Scope<Impl> m_Impl;
    };
}
