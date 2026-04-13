#pragma once

#include "Graphics/Renderer2D.h"

#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsPipeline.h"
#include "Graphics/Shader.h"
#include "Graphics/TextureResource.h"
#include "Graphics/VertexLayout.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace Life::Detail
{
    inline constexpr uint32_t Renderer2DMaxQuads = 16384;
    inline constexpr uint32_t Renderer2DBufferVersionCount = 4;
    inline constexpr uint32_t Renderer2DStaticQuadVertexCount = 6;

    struct Renderer2DSceneConstants
    {
        glm::mat4 ViewProjection{ 1.0f };
    };

    struct Renderer2DQuadBatchRange
    {
        const TextureResource* Texture = nullptr;
        uint32_t InstanceOffset = 0;
        uint32_t InstanceCount = 0;
    };

    struct Renderer2DQuadStaticVertex
    {
        glm::vec2 LocalPosition{ 0.0f, 0.0f };
        glm::vec2 LocalTexCoord{ 0.0f, 0.0f };
    };

    struct Renderer2DQuadInstanceData
    {
        glm::vec4 QuadCenter{ 0.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 QuadXAxis{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec4 QuadYAxis{ 0.0f, 1.0f, 0.0f, 0.0f };
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 TexRect{ 0.0f, 0.0f, 1.0f, 1.0f };
    };
}

namespace Life
{
    struct Renderer2D::Impl
    {
        Scope<GraphicsBuffer> QuadVertexBuffer;
        std::vector<Scope<GraphicsBuffer>> InstanceBuffers;
        std::vector<Scope<GraphicsBuffer>> SceneConstantBuffers;
        Scope<GraphicsPipeline> Pipeline;
        Shader* VertexShader = nullptr;
        Shader* PixelShader = nullptr;
        VertexLayout Layout;
        std::vector<Detail::Renderer2DQuadInstanceData> Instances;
        std::vector<Detail::Renderer2DQuadBatchRange> Batches;
        Statistics Stats{};
        Scope<TextureResource> WhiteTexture;
        Scope<TextureResource> ErrorTexture;
        GraphicsBuffer* ActiveInstanceBuffer = nullptr;
        GraphicsBuffer* ActiveSceneConstantBuffer = nullptr;
        uint32_t ActiveBufferVersion = Detail::Renderer2DBufferVersionCount - 1u;
        uint32_t QueuedQuadCount = 0;
        bool ResourcesReady = false;
        bool ReportedInitializationFailure = false;
        bool SceneActive = false;
    };
}
