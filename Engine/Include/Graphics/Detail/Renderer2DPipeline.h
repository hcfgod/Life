#pragma once

namespace Life
{
    class Renderer2D;

    namespace Detail
    {
        class Renderer2DPipeline
        {
        public:
            explicit Renderer2DPipeline(Renderer2D& renderer2D);

            bool AcquireShaderResources();
            bool AcquirePipelineState();

        private:
            Renderer2D& m_Renderer2D;
        };
    }
}
