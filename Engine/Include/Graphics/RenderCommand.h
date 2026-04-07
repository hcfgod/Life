#pragma once

#include "Graphics/GraphicsTypes.h"

#include <cstdint>

namespace Life
{
    class GraphicsBuffer;
    class GraphicsPipeline;
    class Renderer;
    class TextureResource;

    class RenderCommand
    {
    public:
        RenderCommand() = delete;

        static void Clear(Renderer& renderer, float r, float g, float b, float a = 1.0f);
        static void SetViewport(Renderer& renderer, float x, float y, float width, float height);
        static void SetScissor(Renderer& renderer, int32_t x, int32_t y, uint32_t width, uint32_t height);

        static void Draw(Renderer& renderer,
                         GraphicsPipeline& pipeline,
                         GraphicsBuffer& vertexBuffer,
                         const DrawParameters& drawParameters = {},
                         const TextureResource* texture = nullptr,
                         const GraphicsBuffer* sceneConstants = nullptr);

        static void DrawIndexed(Renderer& renderer,
                                GraphicsPipeline& pipeline,
                                GraphicsBuffer& vertexBuffer,
                                GraphicsBuffer& indexBuffer,
                                const IndexedDrawParameters& drawParameters = {});
    };
}
