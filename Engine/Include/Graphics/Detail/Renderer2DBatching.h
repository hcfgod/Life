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
            struct QuadSubmission
            {
                glm::vec3 Center{};
                glm::vec3 XAxis{};
                glm::vec3 YAxis{};
                glm::vec4 Color{ 1.0f };
                glm::vec2 UVMin{};
                glm::vec2 UVMax{ 1.0f, 1.0f };
                const TextureResource* Texture = nullptr;
            };

            explicit Renderer2DBatching(Renderer2D& renderer2D);

            void AdvanceActiveBufferVersion() noexcept;
            bool UpdateSceneConstants(const glm::mat4& viewProjection);
            void ResetQueuedDraws() noexcept;
            void PushQuad(const QuadSubmission& quad);

        private:
            Renderer2D& m_Renderer2D;
        };
    }
}
