#pragma once

namespace Life
{
    class Renderer2D;

    namespace Detail
    {
        class Renderer2DSubmission
        {
        public:
            explicit Renderer2DSubmission(Renderer2D& renderer2D);

            void SubmitQueuedDraws();

        private:
            Renderer2D& m_Renderer2D;
        };
    }
}
