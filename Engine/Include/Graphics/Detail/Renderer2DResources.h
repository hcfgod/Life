#pragma once

namespace Life
{
    class Renderer2D;

    namespace Detail
    {
        class Renderer2DResources
        {
        public:
            explicit Renderer2DResources(Renderer2D& renderer2D);

            bool EnsureResourcesReady();
            void InvalidateResources() noexcept;

        private:
            bool ValidateResourceState() const noexcept;
            bool AcquireBootstrapResources();

            Renderer2D& m_Renderer2D;
        };
    }
}
