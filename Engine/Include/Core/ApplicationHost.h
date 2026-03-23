#pragma once

#include "Core/Application.h"
#include "Core/ApplicationEventRouter.h"
#include "Core/ApplicationRuntime.h"
#include "Core/Memory.h"
#include "Core/ServiceRegistry.h"
#include "Core/Window.h"

namespace Life
{
    class ApplicationHost
    {
    public:
        explicit ApplicationHost(Scope<Application> application);
        ApplicationHost(Scope<Application> application, Scope<ApplicationRuntime> runtime);
        ~ApplicationHost();

        void Initialize();
        void RunFrame(float timestep);
        void HandleEvent(Event& event);
        void Shutdown();
        void Finalize();

        bool IsRunning() const { return m_Running; }
        bool IsInitialized() const { return m_Initialized; }

        Application& GetApplication() { return *m_Application; }
        const Application& GetApplication() const { return *m_Application; }
        ApplicationRuntime& GetRuntime() { return *m_Runtime; }
        const ApplicationRuntime& GetRuntime() const { return *m_Runtime; }
        Window& GetWindow() { return *m_Window; }
        const Window& GetWindow() const { return *m_Window; }
        ApplicationContext& GetContext() { return m_Context; }
        const ApplicationContext& GetContext() const { return m_Context; }
        ApplicationEventRouter& GetEventRouter() { return m_EventRouter; }
        const ApplicationEventRouter& GetEventRouter() const { return m_EventRouter; }
        ServiceRegistry& GetServices() { return m_Services; }
        const ServiceRegistry& GetServices() const { return m_Services; }

    private:
        Scope<Application> m_Application;
        Scope<ApplicationRuntime> m_Runtime;
        Scope<Window> m_Window;
        ApplicationContext m_Context;
        ApplicationEventRouter m_EventRouter;
        ServiceRegistry m_Services;
        bool m_Running = false;
        bool m_Initialized = false;
    };

    Scope<ApplicationHost> CreateApplicationHost(ApplicationCommandLineArgs args);
    Scope<ApplicationHost> CreateApplicationHost(ApplicationCommandLineArgs args, Scope<ApplicationRuntime> runtime);
}
