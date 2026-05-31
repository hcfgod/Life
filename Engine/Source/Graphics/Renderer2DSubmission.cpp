#include "Core/LifePCH.h"
#include "Graphics/Detail/Renderer2DSubmission.h"

#include "Graphics/Detail/Renderer2DDetail.h"

#include "Graphics/Detail/Renderer2DBatching.h"
#include "Graphics/Detail/Renderer2DResources.h"
#include "Graphics/Renderer2D.h"
#include "Core/Log.h"
#include "Graphics/GraphicsBuffer.h"
#include "Graphics/RenderCommand.h"
#include "Graphics/Renderer.h"

#include <algorithm>

namespace Life::Detail
{
    Renderer2DSubmission::Renderer2DSubmission(Renderer2D& renderer2D)
        : m_Renderer2D(renderer2D)
    {
    }

    void Renderer2DSubmission::SubmitQueuedDraws()
    {
        const bool requiresOpaquePipeline = std::any_of(
            m_Renderer2D.m_Impl->Batches.begin(),
            m_Renderer2D.m_Impl->Batches.end(),
            [](const Renderer2DQuadBatchRange& batch) { return batch.Opaque; });
        const bool requiresTransparentPipeline = std::any_of(
            m_Renderer2D.m_Impl->Batches.begin(),
            m_Renderer2D.m_Impl->Batches.end(),
            [](const Renderer2DQuadBatchRange& batch) { return !batch.Opaque; });

        if ((!m_Renderer2D.m_Impl->OpaquePipeline && requiresOpaquePipeline) ||
            (!m_Renderer2D.m_Impl->TransparentPipeline && requiresTransparentPipeline) ||
            !m_Renderer2D.m_Impl->QuadVertexBuffer ||
            !m_Renderer2D.m_Impl->ActiveInstanceBuffer ||
            !m_Renderer2D.m_Impl->ActiveSceneConstantBuffer ||
            m_Renderer2D.m_Impl->Batches.empty() ||
            m_Renderer2D.m_Impl->Instances.empty())
        {
            Renderer2DBatching batching(m_Renderer2D);
            batching.ResetQueuedDraws();
            return;
        }

        const uint32_t instanceDataSize = static_cast<uint32_t>(m_Renderer2D.m_Impl->Instances.size() * sizeof(Renderer2DQuadInstanceData));
        if (!m_Renderer2D.m_Impl->ActiveInstanceBuffer->SetData(m_Renderer2D.m_Renderer.GetGraphicsDevice(), m_Renderer2D.m_Impl->Instances.data(), instanceDataSize))
        {
            LOG_CORE_ERROR("Renderer2D failed to upload queued instance data.");
            Renderer2DResources resources(m_Renderer2D);
            resources.InvalidateResources();
            return;
        }

        for (const Renderer2DQuadBatchRange& batch : m_Renderer2D.m_Impl->Batches)
        {
            GraphicsPipeline* pipeline = batch.Opaque
                ? m_Renderer2D.m_Impl->OpaquePipeline.get()
                : m_Renderer2D.m_Impl->TransparentPipeline.get();
            if (pipeline == nullptr)
                continue;

            DrawParameters drawParameters;
            drawParameters.VertexCount = Renderer2DStaticQuadVertexCount;
            drawParameters.InstanceCount = batch.InstanceCount;
            drawParameters.InstanceOffset = batch.InstanceOffset;

            RenderCommand::Draw(m_Renderer2D.m_Renderer,
                                *pipeline,
                                {
                                    VertexBufferBindingView{ m_Renderer2D.m_Impl->QuadVertexBuffer.get(), 0, 0 },
                                    VertexBufferBindingView{ m_Renderer2D.m_Impl->ActiveInstanceBuffer, 1, 0 }
                                },
                                drawParameters,
                                batch.Texture,
                                m_Renderer2D.m_Impl->ActiveSceneConstantBuffer);
        }

        m_Renderer2D.m_Impl->Stats.DrawCalls += static_cast<uint32_t>(m_Renderer2D.m_Impl->Batches.size());
        m_Renderer2D.m_Impl->Stats.QuadCount += m_Renderer2D.m_Impl->QueuedQuadCount;

        Renderer2DBatching batching(m_Renderer2D);
        batching.ResetQueuedDraws();
    }
}
