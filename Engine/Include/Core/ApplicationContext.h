#pragma once

#include "Core/ApplicationRuntime.h"
#include "Core/ServiceRegistry.h"

#include <functional>

namespace Life
{
    class ApplicationContext
    {
    public:
        struct StateBinding
        {
            bool& Running;
            bool& Initialized;
        };

        ApplicationContext() = default;

        void Bind(
            Window& window,
            ApplicationRuntime& runtime,
            ServiceRegistry& services,
            StateBinding stateBinding,
            std::function<void()> initializeCallback,
            std::function<void(float)> runFrameCallback,
            std::function<void()> shutdownCallback,
            std::function<void()> finalizeCallback);

        bool IsBound() const;

        void Initialize();
        void RunFrame(float timestep);
        void RequestShutdown();
        void Finalize();

        bool IsRunning() const;
        bool IsInitialized() const;

        ApplicationRuntime& GetRuntime();
        const ApplicationRuntime& GetRuntime() const;
        Window& GetWindow();
        const Window& GetWindow() const;
        ServiceRegistry& GetServices();
        const ServiceRegistry& GetServices() const;

        template<typename TService>
        TService& GetService()
        {
            return GetServices().Get<TService>();
        }

        template<typename TService>
        const TService& GetService() const
        {
            return GetServices().Get<TService>();
        }

        template<typename TService>
        TService* TryGetService()
        {
            return GetServices().TryGet<TService>();
        }

        template<typename TService>
        const TService* TryGetService() const
        {
            return GetServices().TryGet<TService>();
        }

        template<typename TService>
        bool HasService() const
        {
            return GetServices().Has<TService>();
        }

    private:
        Window* m_Window = nullptr;
        ApplicationRuntime* m_Runtime = nullptr;
        ServiceRegistry* m_Services = nullptr;
        bool* m_Running = nullptr;
        bool* m_Initialized = nullptr;
        std::function<void()> m_InitializeCallback;
        std::function<void(float)> m_RunFrameCallback;
        std::function<void()> m_ShutdownCallback;
        std::function<void()> m_FinalizeCallback;
    };
}
