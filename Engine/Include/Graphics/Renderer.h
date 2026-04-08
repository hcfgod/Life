#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/ShaderLibrary.h"
#include "Graphics/TextureResource.h"
#include "Core/Memory.h"

#include <cstdint>
#include <initializer_list>
#include <glm/glm.hpp>

namespace nvrhi
{
    class ICommandList;
    class IFramebuffer;
    class ITexture;
}

namespace Life
{
    class GraphicsDevice;
    class TextureResource;

    struct RendererStats
    {
        uint32_t DrawCalls = 0;
        uint32_t VerticesSubmitted = 0;
        uint32_t IndicesSubmitted = 0;
    };

    struct SceneData
    {
        glm::mat4 ViewMatrix = glm::mat4(1.0f);
        glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
        glm::mat4 ViewProjectionMatrix = glm::mat4(1.0f);
    };

    class Renderer
    {
    public:
        explicit Renderer(GraphicsDevice& graphicsDevice);
        ~Renderer() noexcept;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        GraphicsDevice& GetGraphicsDevice() { return m_GraphicsDevice; }
        const GraphicsDevice& GetGraphicsDevice() const { return m_GraphicsDevice; }
        ShaderLibrary& GetShaderLibrary() { return m_ShaderLibrary; }
        const ShaderLibrary& GetShaderLibrary() const { return m_ShaderLibrary; }
        const RendererStats& GetStats() const noexcept { return m_Stats; }

        void BeginScene(const glm::mat4& viewProjection);
        void BeginScene(const glm::mat4& view, const glm::mat4& projection);
        void EndScene();
        bool IsInScene() const noexcept { return m_InScene; }

        void Clear(float r, float g, float b, float a = 1.0f);
        void SetViewport(float x, float y, float width, float height);
        void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

        void Submit(GraphicsPipeline& pipeline,
                    std::initializer_list<VertexBufferBindingView> vertexBuffers,
                    const DrawParameters& drawParameters = {},
                    const TextureResource* texture = nullptr,
                    const GraphicsBuffer* sceneConstants = nullptr);

        void Submit(GraphicsPipeline& pipeline,
                    GraphicsBuffer& vertexBuffer,
                    const DrawParameters& drawParameters = {},
                    const TextureResource* texture = nullptr,
                    const GraphicsBuffer* sceneConstants = nullptr);

        void SubmitIndexed(GraphicsPipeline& pipeline,
                           GraphicsBuffer& vertexBuffer,
                           GraphicsBuffer& indexBuffer,
                           const IndexedDrawParameters& drawParameters = {});

        bool PushRenderTarget(TextureResource& colorTarget);
        void PopRenderTarget() noexcept;
        void SetRenderTarget(TextureResource* colorTarget);
        TextureResource* GetRenderTarget() const noexcept;
        FramebufferExtent GetFramebufferExtent() const;

        Scope<GraphicsPipeline> CreatePipeline(const GraphicsPipelineDescription& desc);

        nvrhi::IFramebuffer* GetCurrentFramebuffer();

        void ResetStats() noexcept;

    private:
        void EnsureFramebuffer();

        GraphicsDevice& m_GraphicsDevice;
        ShaderLibrary m_ShaderLibrary;
        RendererStats m_Stats;
        SceneData m_SceneData;
        bool m_InScene = false;

        struct Impl;
        Scope<Impl> m_Impl;
    };
}
