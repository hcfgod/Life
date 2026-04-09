#include "Core/LifePCH.h"
#include "Graphics/SceneRenderer2D.h"

#include "Assets/TextureAsset.h"
#include "Graphics/Camera.h"
#include "Graphics/SceneSurface.h"

namespace Life
{
    SceneRenderer2D::SceneRenderer2D(Renderer2D& renderer2D)
        : m_Renderer2D(renderer2D)
    {
    }

    bool SceneRenderer2D::Render(const Scene2D& scene)
    {
        if (scene.Camera == nullptr)
            return false;

        m_Renderer2D.BeginScene(*scene.Camera);
        if (!m_Renderer2D.IsSceneActive())
            return false;

        SubmitScene(m_Renderer2D, scene);
        m_Renderer2D.EndScene();
        return true;
    }

    bool SceneRenderer2D::RenderToSurface(SceneSurface& surface, const Scene2D& scene)
    {
        if (scene.Camera == nullptr)
            return false;

        if (!surface.BeginScene2D(*scene.Camera))
            return false;

        SubmitScene(surface.GetRenderer2D(), scene);
        surface.EndScene2D();
        return true;
    }

    void SceneRenderer2D::SubmitScene(Renderer2D& renderer2D, const Scene2D& scene)
    {
        for (const QuadCommand& quad : scene.Quads)
        {
            if (quad.TextureAsset != nullptr)
            {
                renderer2D.DrawRotatedQuad(quad.Position, quad.Size, quad.RotationRadians, *quad.TextureAsset, quad.Color);
            }
            else if (quad.Texture != nullptr)
            {
                renderer2D.DrawRotatedQuad(quad.Position, quad.Size, quad.RotationRadians, quad.Texture, quad.Color);
            }
            else
            {
                renderer2D.DrawRotatedQuad(quad.Position, quad.Size, quad.RotationRadians, quad.Color);
            }
        }
    }
}
