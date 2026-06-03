#include "Core/LifePCH.h"
#include "Graphics/SceneRenderer2D.h"

#include "Assets/TextureAsset.h"
#include "Graphics/Camera.h"
#include "Graphics/SceneSurface.h"
#include "Scene/Scene.h"

#include <algorithm>

namespace Life
{
    namespace
    {
        float ResolveCameraDepth(const SceneRenderer2D::Scene2D& scene, const SceneRenderer2D::QuadCommand& quad)
        {
            if (scene.Camera == nullptr)
                return quad.Position.z;

            const glm::vec4 viewPosition = scene.Camera->GetViewMatrix() * glm::vec4(quad.Position, 1.0f);
            return viewPosition.z;
        }

        std::size_t ResolveSortingLayerPriority(const Scene& scene, std::string_view sortingLayer) noexcept
        {
            const std::size_t resolvedIndex = scene.ResolveSpriteSortingLayerIndex(sortingLayer);
            if (resolvedIndex != 0u || sortingLayer == "Default")
                return resolvedIndex;

            return scene.GetSpriteSortingLayers().size();
        }

        class ScopedRenderer2DDepthTesting final
        {
        public:
            ScopedRenderer2DDepthTesting(Renderer2D& renderer2D, bool enabled) noexcept
                : m_Renderer2D(renderer2D)
                , m_PreviousEnabled(renderer2D.IsDepthTestingEnabled())
            {
                m_Renderer2D.SetDepthTestingEnabled(enabled);
            }

            ~ScopedRenderer2DDepthTesting() noexcept
            {
                m_Renderer2D.SetDepthTestingEnabled(m_PreviousEnabled);
            }

            ScopedRenderer2DDepthTesting(const ScopedRenderer2DDepthTesting&) = delete;
            ScopedRenderer2DDepthTesting& operator=(const ScopedRenderer2DDepthTesting&) = delete;

        private:
            Renderer2D& m_Renderer2D;
            bool m_PreviousEnabled = true;
        };
    }

    SceneRenderer2D::SceneRenderer2D(Renderer2D& renderer2D)
        : m_Renderer2D(renderer2D)
    {
    }

    bool SceneRenderer2D::Render(const Scene2D& scene, const RenderOptions& options)
    {
        if (scene.Camera == nullptr)
            return false;

        ScopedRenderer2DDepthTesting depthTesting(m_Renderer2D, options.EnableDepthTesting);
        m_Renderer2D.BeginScene(*scene.Camera);
        if (!m_Renderer2D.IsSceneActive())
            return false;

        SubmitScene(m_Renderer2D, scene);
        m_Renderer2D.EndScene();
        return true;
    }

    bool SceneRenderer2D::Render(const Scene& scene, const Camera& camera, QuadSortMode sortMode, const RenderOptions& options)
    {
        return Render(BuildScene2D(scene, camera, sortMode), options);
    }

    bool SceneRenderer2D::RenderToSurface(SceneSurface& surface, const Scene2D& scene, const RenderOptions& options)
    {
        if (scene.Camera == nullptr)
            return false;

        ScopedRenderer2DDepthTesting depthTesting(surface.GetRenderer2D(), options.EnableDepthTesting);
        if (!surface.BeginScene2D(*scene.Camera))
            return false;

        SubmitScene(surface.GetRenderer2D(), scene);
        surface.EndScene2D();
        return true;
    }

    bool SceneRenderer2D::RenderToSurface(SceneSurface& surface,
                                          const Scene& scene,
                                          const Camera& camera,
                                          QuadSortMode sortMode,
                                          const RenderOptions& options)
    {
        return RenderToSurface(surface, BuildScene2D(scene, camera, sortMode), options);
    }

    SceneRenderer2D::Scene2D SceneRenderer2D::BuildScene2D(const Scene& scene, const Camera& camera, QuadSortMode sortMode)
    {
        Scene2D renderScene;
        renderScene.Camera = &camera;
        renderScene.SortMode = sortMode;

        const auto view = scene.GetRegistry().view<TransformComponent, SpriteComponent>();
        renderScene.Quads.reserve(view.size_hint());
        for (const entt::entity handle : view)
        {
            const auto& [transform, sprite] = view.get<TransformComponent, SpriteComponent>(handle);
            const Entity entity = scene.WrapEntity(handle);
            if (!entity.IsEnabled())
                continue;
            const glm::mat4 worldTransform = scene.GetWorldTransformMatrix(entity);

            QuadCommand quad;
            quad.Position = glm::vec3(worldTransform[3]);
            quad.Size = sprite.Size;
            quad.XAxis = glm::vec3(worldTransform * glm::vec4(sprite.Size.x, 0.0f, 0.0f, 0.0f));
            quad.YAxis = glm::vec3(worldTransform * glm::vec4(0.0f, sprite.Size.y, 0.0f, 0.0f));
            quad.Color = sprite.Color;
            if (const SpriteRendererComponent* spriteRenderer = entity.TryGetComponent<SpriteRendererComponent>())
            {
                quad.SortingLayerIndex = ResolveSortingLayerPriority(scene, spriteRenderer->SortingLayer);
                quad.SortingOrder = spriteRenderer->SortingOrder;
            }
            quad.RotationRadians = transform.LocalRotation.z;
            quad.UseExplicitAxes = true;
            quad.TextureAsset = sprite.TextureAsset.get();
            renderScene.Quads.push_back(quad);
        }

        return renderScene;
    }

    std::vector<const SceneRenderer2D::QuadCommand*> SceneRenderer2D::BuildSubmissionOrder(const Scene2D& scene)
    {
        std::vector<const QuadCommand*> orderedQuads;
        orderedQuads.reserve(scene.Quads.size());
        std::vector<size_t> orderedIndices(scene.Quads.size());
        for (size_t index = 0; index < orderedIndices.size(); ++index)
            orderedIndices[index] = index;

        std::stable_sort(
            orderedIndices.begin(),
            orderedIndices.end(),
            [&scene](size_t leftIndex, size_t rightIndex)
            {
                const QuadCommand& left = scene.Quads[leftIndex];
                const QuadCommand& right = scene.Quads[rightIndex];
                if (left.SortingLayerIndex != right.SortingLayerIndex)
                    return left.SortingLayerIndex < right.SortingLayerIndex;
                if (left.SortingOrder != right.SortingOrder)
                    return left.SortingOrder < right.SortingOrder;

                switch (scene.SortMode)
                {
                    case QuadSortMode::BackToFront:
                        return ResolveCameraDepth(scene, left) < ResolveCameraDepth(scene, right);
                    case QuadSortMode::FrontToBack:
                        return ResolveCameraDepth(scene, left) > ResolveCameraDepth(scene, right);
                    case QuadSortMode::SubmissionOrder:
                    default:
                        return leftIndex < rightIndex;
                }
            });

        for (const size_t index : orderedIndices)
            orderedQuads.push_back(&scene.Quads[index]);

        return orderedQuads;
    }

    void SceneRenderer2D::SubmitScene(Renderer2D& renderer2D, const Scene2D& scene)
    {
        const std::vector<const QuadCommand*> orderedQuads = BuildSubmissionOrder(scene);
        for (size_t index = 0; index < orderedQuads.size(); ++index)
        {
            const QuadCommand* quad = orderedQuads[index];
            if (quad->UseExplicitAxes)
            {
                if (quad->TextureAsset != nullptr)
                {
                    renderer2D.DrawQuad(quad->Position, quad->XAxis, quad->YAxis, *quad->TextureAsset, quad->Color);
                }
                else if (quad->Texture != nullptr)
                {
                    renderer2D.DrawQuad(quad->Position, quad->XAxis, quad->YAxis, quad->Texture, quad->Color);
                }
                else
                {
                    renderer2D.DrawQuad(quad->Position, quad->XAxis, quad->YAxis, quad->Color);
                }
            }
            else if (quad->TextureAsset != nullptr)
            {
                renderer2D.DrawRotatedQuad(quad->Position, quad->Size, quad->RotationRadians, *quad->TextureAsset, quad->Color);
            }
            else if (quad->Texture != nullptr)
            {
                renderer2D.DrawRotatedQuad(quad->Position, quad->Size, quad->RotationRadians, quad->Texture, quad->Color);
            }
            else
            {
                renderer2D.DrawRotatedQuad(quad->Position, quad->Size, quad->RotationRadians, quad->Color);
            }

        }
    }
}
