#pragma once

namespace Life
{
    class ApplicationHost;
    struct ApplicationSpecification;

    namespace Detail
    {
        class ApplicationHostConstruction final
        {
        public:
            explicit ApplicationHostConstruction(ApplicationHost& host);

            void Run(const ApplicationSpecification& specification);
            void CleanupFailure() noexcept;
            void Teardown() noexcept;

        private:
            void ConfigureConstructionEnvironment(const ApplicationSpecification& specification);
            void CreatePlatformWindowAndGraphicsDevice(const ApplicationSpecification& specification);
            void AcquireAndRegisterCoreServices(const ApplicationSpecification& specification);
            void CreateAndRegisterHostServices();
            void EnableGlobalServicesAndBindContext();
            void DisableGlobalServices() noexcept;
            void ReleaseSharedSystems() noexcept;
            void UnregisterActiveHost() noexcept;
            void ResetOwnedServices() noexcept;

        private:
            ApplicationHost& m_Host;
        };
    }
}
