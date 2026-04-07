#include "Core/LifePCH.h"
#include "Graphics/RenderCommand.h"
#include "Graphics/Renderer.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/TextureResource.h"

namespace Life
{
    void RenderCommand::Clear(Renderer& renderer, float r, float g, float b, float a)
    {
        renderer.Clear(r, g, b, a);
    }

    void RenderCommand::SetViewport(Renderer& renderer, float x, float y, float width, float height)
    {
        renderer.SetViewport(x, y, width, height);
    }

    void RenderCommand::SetScissor(Renderer& renderer, int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        renderer.SetScissor(x, y, width, height);
    }

    void RenderCommand::Draw(Renderer& renderer,
                             GraphicsPipeline& pipeline,
                             GraphicsBuffer& vertexBuffer,
                             const DrawParameters& drawParameters,
                             const TextureResource* texture)
    {
        renderer.Submit(pipeline, vertexBuffer, drawParameters, texture);
    }

    void RenderCommand::DrawIndexed(Renderer& renderer,
                                    GraphicsPipeline& pipeline,
                                    GraphicsBuffer& vertexBuffer,
                                    GraphicsBuffer& indexBuffer,
                                    const IndexedDrawParameters& drawParameters)
    {
        renderer.SubmitIndexed(pipeline, vertexBuffer, indexBuffer, drawParameters);
    }
}
