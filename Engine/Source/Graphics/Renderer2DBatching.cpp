#include "Core/LifePCH.h"
#include "Graphics/Detail/Renderer2DBatching.h"

#include "Graphics/Detail/Renderer2DDetail.h"

#include "Graphics/Detail/Renderer2DResources.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/Renderer.h"
#include "Core/Log.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/TextureResource.h"

namespace Life::Detail
{
    Renderer2DBatching::Renderer2DBatching(Renderer2D& renderer2D)
        : m_Renderer2D(renderer2D)
    {
    }

    void Renderer2DBatching::AdvanceActiveBufferVersion() noexcept
    {
        m_Renderer2D.m_Impl->ActiveBufferVersion = (m_Renderer2D.m_Impl->ActiveBufferVersion + 1u) % static_cast<uint32_t>(m_Renderer2D.m_Impl->InstanceBuffers.size());
        m_Renderer2D.m_Impl->ActiveInstanceBuffer = m_Renderer2D.m_Impl->InstanceBuffers[m_Renderer2D.m_Impl->ActiveBufferVersion].get();
        m_Renderer2D.m_Impl->ActiveSceneConstantBuffer = m_Renderer2D.m_Impl->SceneConstantBuffers[m_Renderer2D.m_Impl->ActiveBufferVersion].get();
    }

    bool Renderer2DBatching::UpdateSceneConstants(const glm::mat4& viewProjection)
    {
        if (!m_Renderer2D.m_Impl->ActiveSceneConstantBuffer)
            return false;

        Renderer2DSceneConstants sceneConstants;
        sceneConstants.ViewProjection = viewProjection;
        if (!m_Renderer2D.m_Impl->ActiveSceneConstantBuffer->SetData(m_Renderer2D.m_Renderer.GetGraphicsDevice(), &sceneConstants, sizeof(sceneConstants)))
        {
            LOG_CORE_ERROR("Renderer2D failed to upload scene constant data.");
            Renderer2DResources resources(m_Renderer2D);
            resources.InvalidateResources();
            return false;
        }

        return true;
    }

    void Renderer2DBatching::ResetQueuedDraws() noexcept
    {
        m_Renderer2D.m_Impl->Instances.clear();
        m_Renderer2D.m_Impl->Batches.clear();
        m_Renderer2D.m_Impl->QueuedQuadCount = 0;
    }

    void Renderer2DBatching::PushQuad(const QuadSubmission& quad)
    {
        const TextureResource* resolvedTexture = quad.Texture != nullptr ? quad.Texture : m_Renderer2D.m_Impl->ErrorTexture.get();
        if (resolvedTexture == nullptr)
            return;

        if (m_Renderer2D.m_Impl->QueuedQuadCount >= Renderer2DMaxQuads)
            m_Renderer2D.Flush();

        if (m_Renderer2D.m_Impl->Batches.empty() || m_Renderer2D.m_Impl->Batches.back().Texture != resolvedTexture)
        {
            Renderer2DQuadBatchRange batch;
            batch.Texture = resolvedTexture;
            batch.InstanceOffset = static_cast<uint32_t>(m_Renderer2D.m_Impl->Instances.size());
            m_Renderer2D.m_Impl->Batches.push_back(batch);
        }

        Renderer2DQuadInstanceData instance;
        instance.QuadCenter = glm::vec4(quad.Center, 0.0f);
        instance.QuadXAxis = glm::vec4(quad.XAxis, 0.0f);
        instance.QuadYAxis = glm::vec4(quad.YAxis, 0.0f);
        instance.Color = quad.Color;
        instance.TexRect = { quad.UVMin.x, quad.UVMin.y, quad.UVMax.x, quad.UVMax.y };
        m_Renderer2D.m_Impl->Instances.push_back(instance);

        Renderer2DQuadBatchRange& batch = m_Renderer2D.m_Impl->Batches.back();
        batch.InstanceCount++;
        m_Renderer2D.m_Impl->QueuedQuadCount++;
    }
}
