#pragma once

#include "Core/Memory.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Life
{
    class Camera;
    class GraphicsBuffer;
    class GraphicsPipeline;
    class Renderer;

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
        void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationRadians,
                             const glm::vec4& color);

        const Statistics& GetStats() const noexcept;
        void ResetStats() noexcept;

    private:
        void InitializeResources();
        void EnsurePipeline();
        void PushQuad(const glm::mat4& transform, const glm::vec4& color);

        Renderer& m_Renderer;

        struct Impl;
        Scope<Impl> m_Impl;
    };
}
