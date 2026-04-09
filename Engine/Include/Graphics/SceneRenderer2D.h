#pragma once

#include "Graphics/Renderer2D.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Life
{
    namespace Assets
    {
        class TextureAsset;
    }

    class Camera;
    class SceneSurface;
    class TextureResource;

    class SceneRenderer2D
    {
    public:
        struct QuadCommand
        {
            glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
            glm::vec2 Size{ 1.0f, 1.0f };
            glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
            float RotationRadians = 0.0f;
            const TextureResource* Texture = nullptr;
            const Assets::TextureAsset* TextureAsset = nullptr;
        };

        struct Scene2D
        {
            const Camera* Camera = nullptr;
            std::vector<QuadCommand> Quads;

            void Clear()
            {
                Camera = nullptr;
                Quads.clear();
            }
        };

        explicit SceneRenderer2D(Renderer2D& renderer2D);

        SceneRenderer2D(const SceneRenderer2D&) = delete;
        SceneRenderer2D& operator=(const SceneRenderer2D&) = delete;

        bool Render(const Scene2D& scene);
        bool RenderToSurface(SceneSurface& surface, const Scene2D& scene);

        Renderer2D& GetRenderer2D() noexcept { return m_Renderer2D; }
        const Renderer2D& GetRenderer2D() const noexcept { return m_Renderer2D; }
        const Renderer2D::Statistics& GetStats() const noexcept { return m_Renderer2D.GetStats(); }
        void ResetStats() noexcept { m_Renderer2D.ResetStats(); }

    private:
        static void SubmitScene(Renderer2D& renderer2D, const Scene2D& scene);

        Renderer2D& m_Renderer2D;
    };
}
