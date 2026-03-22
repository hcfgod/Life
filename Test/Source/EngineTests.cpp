#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "Core/ApplicationRunner.h"
#include "Engine.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
    class TestWindow final : public Life::Window
    {
    public:
        explicit TestWindow(Life::WindowSpecification specification)
            : m_Specification(std::move(specification))
        {
        }

        const Life::WindowSpecification& GetSpecification() const override
        {
            return m_Specification;
        }

        void* GetNativeHandle() const override
        {
            return nullptr;
        }

    private:
        Life::WindowSpecification m_Specification;
    };

    class TestRuntime final : public Life::ApplicationRuntime
    {
    public:
        Life::Scope<Life::Window> CreatePlatformWindow(const Life::WindowSpecification& specification) override
        {
            ++CreateWindowCount;
            return Life::CreateScope<TestWindow>(specification);
        }

        Life::Scope<Life::Event> PollEvent() override
        {
            ++PollCount;

            if (QueuedEvents.empty())
                return nullptr;

            Life::Scope<Life::Event> event = std::move(QueuedEvents.front());
            QueuedEvents.erase(QueuedEvents.begin());
            return event;
        }

        template<typename TEvent, typename... TArguments>
        void QueueEvent(TArguments&&... arguments)
        {
            QueuedEvents.emplace_back(Life::CreateScope<TEvent>(std::forward<TArguments>(arguments)...));
        }

        int CreateWindowCount = 0;
        int PollCount = 0;
        std::vector<Life::Scope<Life::Event>> QueuedEvents;
    };

    class TestApplication final : public Life::Application
    {
    public:
        explicit TestApplication(Life::Scope<Life::ApplicationRuntime> runtime)
            : Life::Application(CreateSpecification(), std::move(runtime))
        {
        }

        static Life::ApplicationSpecification CreateSpecification()
        {
            Life::ApplicationSpecification specification;
            specification.Name = "Test Application";
            specification.Width = 640;
            specification.Height = 480;
            specification.VSync = false;
            return specification;
        }

        TestRuntime& GetTestRuntime()
        {
            return static_cast<TestRuntime&>(GetRuntime());
        }

        const std::vector<std::string>& GetTrace() const
        {
            return Trace;
        }

        int InitCount = 0;
        int ShutdownCount = 0;
        int UpdateCount = 0;
        bool HandleCloseInOnEvent = false;
        bool ShutdownOnUpdate = false;
        std::vector<std::string> Trace;

    protected:
        void OnInit() override
        {
            ++InitCount;
            Trace.emplace_back("init");
        }

        void OnShutdown() override
        {
            ++ShutdownCount;
            Trace.emplace_back("shutdown");
        }

        void OnUpdate(float timestep) override
        {
            (void)timestep;
            ++UpdateCount;
            Trace.emplace_back("update");

            if (ShutdownOnUpdate)
                Shutdown();
        }

        void OnEvent(Life::Event& event) override
        {
            Trace.emplace_back(std::string("on_event:") + event.GetName());

            if (HandleCloseInOnEvent && event.GetEventType() == Life::WindowCloseEvent::GetStaticType())
                event.Handled = true;
        }
    };
}

TEST_CASE("CreateScope stores values")
{
    Life::Scope<int> value = Life::CreateScope<int>(42);

    CHECK(*value == 42);
}

TEST_CASE("EventDispatcher dispatches typed events")
{
    Life::WindowCloseEvent event;
    bool wasDispatched = false;

    Life::EventDispatcher dispatcher(event);
    dispatcher.Dispatch<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& dispatchedEvent)
    {
        wasDispatched = true;
        CHECK(dispatchedEvent.GetEventType() == Life::EventType::WindowClose);
        return true;
    });

    CHECK(wasDispatched);
    CHECK(event.Handled);
}

TEST_CASE("EventBus dispatches subscribed handlers")
{
    Life::EventBus bus;
    bool wasCalled = false;

    bus.Subscribe<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& event)
    {
        wasCalled = true;
        CHECK(event.GetWidth() == 1600);
        CHECK(event.GetHeight() == 900);
        return true;
    });

    const bool wasHandled = bus.Dispatch<Life::WindowResizeEvent>(1600, 900);

    CHECK(wasCalled);
    CHECK(wasHandled);
}

TEST_CASE("EventBus unsubscribes handlers")
{
    Life::EventBus bus;
    int callCount = 0;

    const Life::EventSubscriptionId subscriptionId = bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        ++callCount;
        CHECK(event.GetEventType() == Life::EventType::WindowClose);
        return false;
    });

    CHECK(bus.Unsubscribe(subscriptionId));
    CHECK_FALSE(bus.Dispatch<Life::WindowCloseEvent>());
    CHECK(callCount == 0);
}

TEST_CASE("WindowResizeEvent stores its dimensions")
{
    Life::WindowResizeEvent event(1600, 900);

    CHECK(event.GetWidth() == 1600);
    CHECK(event.GetHeight() == 900);
    CHECK(event.IsInCategory(Life::EventCategory::Window));
    CHECK(event.IsInCategory(Life::EventCategory::Application));
}

TEST_CASE("Log configuration updates logger names and levels")
{
    const Life::LogSpecification originalSpecification = Life::Log::GetSpecification();

    Life::LogSpecification testSpecification = originalSpecification;
    testSpecification.CoreLoggerName = "TEST_CORE";
    testSpecification.ClientLoggerName = "TEST_CLIENT";
    testSpecification.CoreLevel = spdlog::level::warn;
    testSpecification.ClientLevel = spdlog::level::err;
    testSpecification.EnableFile = false;

    Life::Log::Configure(testSpecification);

    const std::shared_ptr<spdlog::logger> coreLogger = Life::Log::GetCoreLogger();
    const std::shared_ptr<spdlog::logger> clientLogger = Life::Log::GetClientLogger();

    REQUIRE(coreLogger != nullptr);
    REQUIRE(clientLogger != nullptr);
    CHECK(coreLogger->name() == testSpecification.CoreLoggerName);
    CHECK(clientLogger->name() == testSpecification.ClientLoggerName);
    CHECK(coreLogger->level() == testSpecification.CoreLevel);
    CHECK(clientLogger->level() == testSpecification.ClientLevel);

    Life::Log::Configure(originalSpecification);
}

TEST_CASE("Application startup preserves init close shutdown ordering")
{
    Life::Log::Init();

    TestApplication application(Life::CreateScope<TestRuntime>());
    application.GetTestRuntime().QueueEvent<Life::WindowCloseEvent>();

    application.Startup();

    CHECK(application.InitCount == 1);
    CHECK(application.ShutdownCount == 1);
    CHECK(application.UpdateCount == 0);
    CHECK(application.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent", "shutdown" });
}

TEST_CASE("Application event propagation order is OnEvent then EventBus then built in shutdown")
{
    Life::Log::Init();

    TestApplication application(Life::CreateScope<TestRuntime>());
    application.Initialize();
    application.SubscribeEvent<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        application.Trace.emplace_back(std::string("event_bus:") + event.GetName());
        return false;
    });

    Life::WindowCloseEvent event;
    application.HandleEvent(event);

    CHECK(event.Handled);
    CHECK_FALSE(application.IsRunning());
    CHECK(application.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent", "event_bus:WindowCloseEvent" });

    application.Finalize();
    CHECK(application.ShutdownCount == 1);
}

TEST_CASE("External event pump close event triggers shutdown before update")
{
    Life::Log::Init();

    TestApplication application(Life::CreateScope<TestRuntime>());
    application.Initialize();

    std::vector<Life::Scope<Life::Event>> pendingEvents;
    pendingEvents.emplace_back(Life::CreateScope<Life::WindowCloseEvent>());
    auto lastFrameTime = std::chrono::steady_clock::now();

    CHECK_FALSE(Life::RunApplicationLoopIteration(application, lastFrameTime, pendingEvents, true));
    CHECK(application.GetTestRuntime().PollCount == 0);
    CHECK(application.UpdateCount == 0);
    CHECK(application.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent" });

    application.Finalize();
    CHECK(application.ShutdownCount == 1);
}

TEST_CASE("Handled close events do not trigger built in shutdown")
{
    Life::Log::Init();

    TestApplication application(Life::CreateScope<TestRuntime>());
    application.HandleCloseInOnEvent = true;
    application.Initialize();

    Life::WindowCloseEvent event;
    application.HandleEvent(event);

    CHECK(event.Handled);
    CHECK(application.IsRunning());
    CHECK(application.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent" });

    application.Finalize();
    CHECK(application.ShutdownCount == 1);
}

TEST_CASE("External event pump path delivers queued events without polling runtime")
{
    Life::Log::Init();

    TestApplication application(Life::CreateScope<TestRuntime>());
    application.ShutdownOnUpdate = true;
    application.Initialize();
    application.GetTestRuntime().QueueEvent<Life::WindowCloseEvent>();
    application.SubscribeEvent<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& event)
    {
        application.Trace.emplace_back(std::string("event_bus:") + event.GetName());
        CHECK(event.GetWidth() == 1600);
        CHECK(event.GetHeight() == 900);
        return false;
    });

    std::vector<Life::Scope<Life::Event>> pendingEvents;
    pendingEvents.emplace_back(Life::CreateScope<Life::WindowResizeEvent>(1600, 900));
    auto lastFrameTime = std::chrono::steady_clock::now();

    CHECK_FALSE(Life::RunApplicationLoopIteration(application, lastFrameTime, pendingEvents, true));
    CHECK(application.GetTestRuntime().PollCount == 0);
    CHECK(application.GetTestRuntime().QueuedEvents.size() == 1);
    CHECK(application.UpdateCount == 1);
    CHECK(application.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowResizeEvent", "event_bus:WindowResizeEvent", "update" });

    application.Finalize();
    CHECK(application.ShutdownCount == 1);
}
