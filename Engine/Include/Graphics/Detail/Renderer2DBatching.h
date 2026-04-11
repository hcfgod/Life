#pragma once

#include <glm/glm.hpp>

namespace Life
{
    class Renderer2D;
    class TextureResource;

    namespace Detail
    {
        class Renderer2DBatching
        {
        public:
            explicit Renderer2DBatching(Renderer2D& renderer2D);

            void AdvanceActiveBufferVersion() noexcept;
            bool UpdateSceneConstants(const glm::mat4& viewProjection);
            void ResetQueuedDraws() noexcept;
            void PushQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians, const glm::vec4& color,
                          const glm::vec2& uvMin, const glm::vec2& uvMax, const TextureResource* texture);

        private:
            Renderer2D& m_Renderer2D;
        };
    }
}
