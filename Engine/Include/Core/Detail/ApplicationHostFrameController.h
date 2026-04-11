#pragma once

namespace Life
{
    class ApplicationHost;

    namespace Detail
    {
        class ApplicationHostFrameController final
        {
        public:
            explicit ApplicationHostFrameController(ApplicationHost& host);

            void RunFrame(float timestep);

        private:
            void UpdateInputCaptureState() noexcept;
            bool TryBeginGraphicsFrame() noexcept;
            void BeginImGuiFramePhase(bool frameStarted);
            void RunApplicationUpdatePhase(float timestep);
            void RunAssetHotReloadPhase();
            void RunLayerUpdatePhase(float timestep);
            void RunLayerRenderPhase(bool frameStarted);
            void RunImGuiRenderPhase(bool frameStarted);
            void RunPresentPhase(bool frameStarted) noexcept;

        private:
            ApplicationHost& m_Host;
        };
    }
}
