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
    class Scene;
    class SceneSurface;
    class TextureResource;

    class SceneRenderer2D
    {
    public:
        enum class QuadSortMode : uint8_t
        {
            SubmissionOrder = 0,
            BackToFront = 1,
            FrontToBack = 2
        };

        struct QuadCommand
        {
            glm::vec3 Position{ 0.0f, 0.0f, 0.0f };
            glm::vec2 Size{ 1.0f, 1.0f };
            glm::vec3 XAxis{ 1.0f, 0.0f, 0.0f };
            glm::vec3 YAxis{ 0.0f, 1.0f, 0.0f };
            glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
            float RotationRadians = 0.0f;
            bool UseExplicitAxes = false;
            const TextureResource* Texture = nullptr;
            const Assets::TextureAsset* TextureAsset = nullptr;
        };

        struct Scene2D
        {
            const Camera* Camera = nullptr;
            QuadSortMode SortMode = QuadSortMode::BackToFront;
            std::vector<QuadCommand> Quads;

            void Clear()
            {
                Camera = nullptr;
                SortMode = QuadSortMode::BackToFront;
                Quads.clear();
            }
        };

        explicit SceneRenderer2D(Renderer2D& renderer2D);

        SceneRenderer2D(const SceneRenderer2D&) = delete;
        SceneRenderer2D& operator=(const SceneRenderer2D&) = delete;

        bool Render(const Scene2D& scene);
        bool Render(const Scene& scene, const Camera& camera, QuadSortMode sortMode = QuadSortMode::BackToFront);
        bool RenderToSurface(SceneSurface& surface, const Scene2D& scene);
        bool RenderToSurface(SceneSurface& surface, const Scene& scene, const Camera& camera, QuadSortMode sortMode = QuadSortMode::BackToFront);
        static std::vector<const QuadCommand*> BuildSubmissionOrder(const Scene2D& scene);

        Renderer2D& GetRenderer2D() noexcept { return m_Renderer2D; }
        const Renderer2D& GetRenderer2D() const noexcept { return m_Renderer2D; }
        const Renderer2D::Statistics& GetStats() const noexcept { return m_Renderer2D.GetStats(); }
        void ResetStats() noexcept { m_Renderer2D.ResetStats(); }

    private:
        static Scene2D BuildScene2D(const Scene& scene, const Camera& camera, QuadSortMode sortMode);
        static void SubmitScene(Renderer2D& renderer2D, const Scene2D& scene);

        Renderer2D& m_Renderer2D;
    };
}
