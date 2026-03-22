#pragma once

#include "Core/ApplicationRuntime.h"
#include "Core/Events/Event.h"
#include "Core/Log.h"
#include "Core/Memory.h"
#include "Core/Window.h"

#include <cstdint>
#include <string>
#include <utility>

namespace Life
{
    struct ApplicationCommandLineArgs
    {
        int Count = 0;
        char** Args = nullptr;

        char* operator[](int index) const
        {
            return index >= 0 && index < Count ? Args[index] : nullptr;
        }
    };

    struct ApplicationSpecification
    {
        std::string Name = "Life Application";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool VSync = true;
        LogSpecification Logging;
        ApplicationCommandLineArgs CommandLineArgs;
    };

    class Application
    {
    public:
        explicit Application(ApplicationSpecification specification);
        virtual ~Application();

        void Initialize();
        void Startup();
        void RunFrame(float timestep);
        void HandleEvent(Event& event);

        template<typename TEvent, typename TFunction>
        EventSubscriptionId SubscribeEvent(TFunction&& function)
        {
            return m_EventBus.Subscribe<TEvent>(std::forward<TFunction>(function));
        }

        bool UnsubscribeEvent(EventSubscriptionId subscriptionId)
        {
            return m_EventBus.Unsubscribe(subscriptionId);
        }

        void DispatchEvent(Event& event)
        {
            HandleEvent(event);
        }

        template<typename TEvent, typename... TArguments>
        bool DispatchEvent(TArguments&&... arguments)
        {
            static_assert(std::is_base_of_v<Event, TEvent>);

            TEvent event(std::forward<TArguments>(arguments)...);
            HandleEvent(event);
            return event.Handled;
        }

        void Shutdown();
        void Finalize();

        bool IsRunning() const { return m_Running; }
        bool IsInitialized() const { return m_Initialized; }

        const ApplicationSpecification& GetSpecification() const { return m_Specification; }
        ApplicationRuntime& GetRuntime() { return *m_Runtime; }
        const ApplicationRuntime& GetRuntime() const { return *m_Runtime; }
        Window& GetWindow() { return *m_Window; }
        const Window& GetWindow() const { return *m_Window; }

    protected:
        Application(ApplicationSpecification specification, Scope<ApplicationRuntime> runtime);

        virtual void OnInit() {}
        virtual void OnShutdown() {}
        virtual void OnUpdate(float timestep) {}
        virtual void OnEvent(Event& event) {}

    private:
        ApplicationSpecification m_Specification;
        Scope<ApplicationRuntime> m_Runtime;
        Scope<Window> m_Window;
        EventBus m_EventBus;
        bool m_Running = false;
        bool m_Initialized = false;
    };

    Scope<Application> CreateApplication(ApplicationCommandLineArgs args);
}
