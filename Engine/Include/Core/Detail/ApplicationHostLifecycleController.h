#pragma once

namespace Life
{
    class ApplicationHost;

    namespace Detail
    {
        class ApplicationHostLifecycleController final
        {
        public:
            explicit ApplicationHostLifecycleController(ApplicationHost& host);

            void Initialize();
            void Finalize();

        private:
            void ClearLayersAfterInitializationFailure() noexcept;

        private:
            ApplicationHost& m_Host;
        };
    }
}
