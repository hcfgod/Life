#pragma once

#include "Core/Application.h"
#include "Core/ApplicationEventRouter.h"
#include "Core/Input/InputSystem.h"
#include "Core/LayerStack.h"
#include "Core/ApplicationRuntime.h"
#include "Core/Memory.h"
#include "Core/ServiceRegistry.h"
#include "Core/Window.h"
#include "Graphics/CameraManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/Renderer.h"
#include "Graphics/Renderer2D.h"

namespace Life
{
    class ApplicationHost
    {
    public:
        explicit ApplicationHost(Scope<Application> application);
        ApplicationHost(Scope<Application> application, Scope<ApplicationRuntime> runtime);
        ~ApplicationHost() noexcept;

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
        LayerStack& GetLayerStack() { return m_LayerStack; }
        const LayerStack& GetLayerStack() const { return m_LayerStack; }
        InputSystem& GetInputSystem() { return m_InputSystem; }
        const InputSystem& GetInputSystem() const { return m_InputSystem; }
        ServiceRegistry& GetServices() { return m_Services; }
        const ServiceRegistry& GetServices() const { return m_Services; }
        GraphicsDevice* GetGraphicsDevice() { return m_GraphicsDevice.get(); }
        const GraphicsDevice* GetGraphicsDevice() const { return m_GraphicsDevice.get(); }
        Renderer* GetRenderer() { return m_Renderer.get(); }
        const Renderer* GetRenderer() const { return m_Renderer.get(); }
        CameraManager* GetCameraManager() { return m_CameraManager.get(); }
        const CameraManager* GetCameraManager() const { return m_CameraManager.get(); }
        Renderer2D* GetRenderer2D() { return m_Renderer2D.get(); }
        const Renderer2D* GetRenderer2D() const { return m_Renderer2D.get(); }

    private:
        Scope<Application> m_Application;
        Scope<ApplicationRuntime> m_Runtime;
        Scope<Window> m_Window;
        Scope<GraphicsDevice> m_GraphicsDevice;
        Scope<Renderer> m_Renderer;
        Scope<CameraManager> m_CameraManager;
        Scope<Renderer2D> m_Renderer2D;
        ApplicationContext m_Context;
        ApplicationEventRouter m_EventRouter;
        LayerStack m_LayerStack;
        InputSystem m_InputSystem;
        ServiceRegistry m_Services;
        bool m_Running = false;
        bool m_Initialized = false;
        bool m_Finalizing = false;
        bool m_SharedSystemsAcquired = false;
        bool m_GlobalServicesRegistered = false;
        bool m_RegisteredAsActiveHost = false;
    };

    Scope<ApplicationHost> CreateApplicationHost(ApplicationCommandLineArgs args);
    Scope<ApplicationHost> CreateApplicationHost(ApplicationCommandLineArgs args, Scope<ApplicationRuntime> runtime);
}
