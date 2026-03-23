#pragma once

#include "Core/ApplicationContext.h"
#include "Core/CrashDiagnostics.h"
#include "Core/ApplicationEventRouter.h"
#include "Core/Events/Event.h"
#include "Core/Log.h"
#include "Core/Memory.h"

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
        CrashReportingSpecification CrashReporting;
        ApplicationCommandLineArgs CommandLineArgs;
    };

    class Application
    {
    public:
        explicit Application(ApplicationSpecification specification);
        virtual ~Application();

        void Initialize();
        void RunFrame(float timestep);
        void HandleEvent(Event& event);

        template<typename TEvent, typename TFunction>
        EventSubscriptionId SubscribeEvent(TFunction&& function)
        {
            return RequireEventRouter().Subscribe<TEvent>(std::forward<TFunction>(function));
        }

        bool UnsubscribeEvent(EventSubscriptionId subscriptionId)
        {
            return RequireEventRouter().Unsubscribe(subscriptionId);
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

        bool IsRunning() const;
        bool IsInitialized() const;

        const ApplicationSpecification& GetSpecification() const { return m_Specification; }
        Window& GetWindow();
        const Window& GetWindow() const;
        ApplicationContext& GetContext();
        const ApplicationContext& GetContext() const;

    protected:
        virtual void OnInit() {}
        virtual void OnShutdown() {}
        virtual void OnUpdate(float timestep) {}
        virtual void OnEvent(Event& event) {}

    private:
        friend class ApplicationEventRouter;
        friend class ApplicationHost;

        void BindHost(ApplicationContext& context, ApplicationEventRouter& eventRouter);

        ApplicationContext& RequireContext();
        const ApplicationContext& RequireContext() const;
        ApplicationEventRouter& RequireEventRouter();
        const ApplicationEventRouter& RequireEventRouter() const;

        void OnHostInitialize();
        void OnHostRunFrame(float timestep);
        void OnHostFinalize();

        ApplicationSpecification m_Specification;
        ApplicationContext* m_Context = nullptr;
        ApplicationEventRouter* m_EventRouter = nullptr;
    };

    Scope<Application> CreateApplication(ApplicationCommandLineArgs args);
}
