#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "Core/ApplicationHost.h"
#include "Core/ApplicationRunner.h"
#include "Engine.h"
#include "Platform/SDL/SDLEvent.h"

#include <filesystem>
#include <fstream>
#include <sstream>
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
        TestApplication()
            : Life::Application(CreateSpecification())
        {
        }

        static Life::ApplicationSpecification CreateSpecification()
        {
            Life::ApplicationSpecification specification;
            specification.Name = "Test Application";
            specification.Width = 640;
            specification.Height = 480;
            specification.VSync = false;
            specification.Concurrency.JobWorkerCount = 1;
            specification.Concurrency.AsyncWorkerCount = 1;
            return specification;
        }

        TestRuntime& GetTestRuntime()
        {
            return static_cast<TestRuntime&>(GetContext().GetRuntime());
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

    struct TestService
    {
        int Value = 0;
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

TEST_CASE("EventBus subscriber mutations apply on the next dispatch")
{
    Life::EventBus bus;
    Life::EventSubscriptionId secondSubscriptionId = 0;
    bool removedSecondSubscription = false;
    bool addedLateSubscription = false;
    int firstCallCount = 0;
    int secondCallCount = 0;
    int lateCallCount = 0;

    bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        CHECK(event.GetEventType() == Life::EventType::WindowClose);
        ++firstCallCount;

        if (!removedSecondSubscription)
        {
            CHECK(bus.Unsubscribe(secondSubscriptionId));
            removedSecondSubscription = true;
        }

        if (!addedLateSubscription)
        {
            bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& lateEvent)
            {
                CHECK(lateEvent.GetEventType() == Life::EventType::WindowClose);
                ++lateCallCount;
                return false;
            });
            addedLateSubscription = true;
        }

        return false;
    });

    secondSubscriptionId = bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        CHECK(event.GetEventType() == Life::EventType::WindowClose);
        ++secondCallCount;
        return false;
    });

    CHECK_FALSE(bus.Dispatch<Life::WindowCloseEvent>());
    CHECK(firstCallCount == 1);
    CHECK(secondCallCount == 1);
    CHECK(lateCallCount == 0);

    CHECK_FALSE(bus.Dispatch<Life::WindowCloseEvent>());
    CHECK(firstCallCount == 2);
    CHECK(secondCallCount == 1);
    CHECK(lateCallCount == 1);
}

TEST_CASE("ServiceRegistry registers and unregisters typed services")
{
    Life::ServiceRegistry registry;
    TestService service{ 42 };

    CHECK_FALSE(registry.Has<TestService>());
    CHECK(registry.TryGet<TestService>() == nullptr);
    CHECK_THROWS_AS(registry.Get<TestService>(), std::logic_error);

    registry.Register<TestService>(service);

    CHECK(registry.Has<TestService>());
    REQUIRE(registry.TryGet<TestService>() != nullptr);
    CHECK(registry.TryGet<TestService>() == &service);
    CHECK(registry.Get<TestService>().Value == 42);

    CHECK(registry.Unregister<TestService>());
    CHECK_FALSE(registry.Has<TestService>());
    CHECK(registry.TryGet<TestService>() == nullptr);
    CHECK_FALSE(registry.Unregister<TestService>());
}

TEST_CASE("WindowResizeEvent stores its dimensions")
{
    Life::WindowResizeEvent event(1600, 900);

    CHECK(event.GetWidth() == 1600);
    CHECK(event.GetHeight() == 900);
    CHECK(event.IsInCategory(Life::EventCategory::Window));
    CHECK(event.IsInCategory(Life::EventCategory::Application));
}

TEST_CASE("Unbound application context operations throw")
{
    Life::ApplicationContext context;

    CHECK_FALSE(context.IsBound());
    CHECK_THROWS_AS(context.Initialize(), std::logic_error);
    CHECK_THROWS_AS(context.RunFrame(0.016f), std::logic_error);
    CHECK_THROWS_AS(context.RequestShutdown(), std::logic_error);
    CHECK_THROWS_AS(context.Finalize(), std::logic_error);
    CHECK_THROWS_AS(context.IsRunning(), std::logic_error);
    CHECK_THROWS_AS(context.IsInitialized(), std::logic_error);
    CHECK_THROWS_AS(context.GetRuntime(), std::logic_error);
    CHECK_THROWS_AS(context.GetWindow(), std::logic_error);
    CHECK_THROWS_AS(context.GetServices(), std::logic_error);
    CHECK_THROWS_AS(context.GetService<TestService>(), std::logic_error);
}

TEST_CASE("Unbound application host dependent operations throw")
{
    TestApplication application;
    Life::WindowCloseEvent event;

    CHECK_FALSE(application.IsRunning());
    CHECK_FALSE(application.IsInitialized());
    CHECK_THROWS_AS(application.Initialize(), std::logic_error);
    CHECK_THROWS_AS(application.RunFrame(0.016f), std::logic_error);
    CHECK_THROWS_AS(application.HandleEvent(event), std::logic_error);
    CHECK_THROWS_AS(application.Shutdown(), std::logic_error);
    CHECK_THROWS_AS(application.Finalize(), std::logic_error);
    CHECK_THROWS_AS(application.GetWindow(), std::logic_error);
    CHECK_THROWS_AS(application.GetContext(), std::logic_error);
    CHECK_THROWS_AS(application.GetServices(), std::logic_error);
    CHECK_THROWS_AS(application.GetService<TestService>(), std::logic_error);
    CHECK_THROWS_AS(application.SubscribeEvent<Life::WindowCloseEvent>([](Life::WindowCloseEvent&) { return false; }), std::logic_error);
    CHECK_THROWS_AS(application.UnsubscribeEvent(1), std::logic_error);
}

TEST_CASE("ApplicationHost registers built-in and custom services")
{
    auto application = Life::CreateScope<TestApplication>();
    auto* applicationInstance = application.get();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());

    CHECK(host->GetServices().Has<Life::ApplicationHost>());
    CHECK(host->GetServices().Has<Life::Application>());
    CHECK(host->GetServices().Has<Life::ApplicationContext>());
    CHECK(host->GetServices().Has<Life::ApplicationEventRouter>());
    CHECK(host->GetServices().Has<Life::JobSystem>());
    CHECK(host->GetServices().Has<Life::Async::AsyncIO>());
    CHECK(host->GetServices().Has<Life::ApplicationRuntime>());
    CHECK(host->GetServices().Has<Life::Window>());

    CHECK(&host->GetServices().Get<Life::ApplicationHost>() == host.get());
    CHECK(&host->GetServices().Get<Life::Application>() == applicationInstance);
    CHECK(&host->GetServices().Get<Life::JobSystem>() == &Life::GetJobSystem());
    CHECK(&host->GetServices().Get<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    CHECK(&host->GetContext().GetService<Life::ApplicationHost>() == host.get());
    CHECK(&host->GetContext().GetService<Life::JobSystem>() == &Life::GetJobSystem());
    CHECK(&applicationInstance->GetService<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    CHECK(&applicationInstance->GetService<Life::ApplicationHost>() == host.get());
    CHECK(&applicationInstance->GetService<Life::Window>() == &host->GetWindow());
    CHECK(&Life::GetServices() == &host->GetServices());

    TestService service{ 7 };
    host->GetServices().Register<TestService>(service);

    CHECK(host->GetContext().HasService<TestService>());
    CHECK(applicationInstance->HasService<TestService>());
    CHECK(applicationInstance->TryGetService<TestService>() == &service);
    CHECK(applicationInstance->GetService<TestService>().Value == 7);
    CHECK(Life::GetServices().TryGet<TestService>() == &service);
}

TEST_CASE("ApplicationHost initializes shared JobSystem and AsyncIO services")
{
    CHECK_FALSE(Life::GetJobSystem().IsInitialized());
    CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());

    {
        auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(Life::GetJobSystem().IsInitialized());
        CHECK(Life::Async::GetAsyncIO().IsInitialized());
        CHECK(&host->GetServices().Get<Life::JobSystem>() == &Life::GetJobSystem());
        CHECK(&host->GetServices().Get<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    }

    CHECK_FALSE(Life::GetJobSystem().IsInitialized());
    CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());
}

TEST_CASE("Nested ApplicationHost instances share JobSystem and AsyncIO lifetime")
{
    auto outerHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    REQUIRE(Life::GetJobSystem().IsInitialized());
    REQUIRE(Life::Async::GetAsyncIO().IsInitialized());

    {
        auto innerHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(&outerHost->GetServices().Get<Life::JobSystem>() == &innerHost->GetServices().Get<Life::JobSystem>());
        CHECK(&outerHost->GetServices().Get<Life::Async::AsyncIO>() == &innerHost->GetServices().Get<Life::Async::AsyncIO>());
    }

    CHECK(Life::GetJobSystem().IsInitialized());
    CHECK(Life::Async::GetAsyncIO().IsInitialized());

    outerHost.reset();

    CHECK_FALSE(Life::GetJobSystem().IsInitialized());
    CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());
}

TEST_CASE("Global service registry falls back after host destruction")
{
    {
        auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(&Life::GetServices() == &host->GetServices());
    }

    CHECK_FALSE(Life::GetServices().Has<Life::ApplicationHost>());
}

TEST_CASE("Global service registry restores the previous host after nested destruction")
{
    auto outerHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    CHECK(&Life::GetServices() == &outerHost->GetServices());

    {
        auto innerHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(&Life::GetServices() == &innerHost->GetServices());
    }

    CHECK(&Life::GetServices() == &outerHost->GetServices());
}

TEST_CASE("Global service registry preserves the current host when an older host is destroyed")
{
    auto firstHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    auto secondHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());

    CHECK(&Life::GetServices() == &secondHost->GetServices());

    firstHost.reset();

    CHECK(&Life::GetServices() == &secondHost->GetServices());

    secondHost.reset();

    CHECK_FALSE(Life::GetServices().Has<Life::ApplicationHost>());
}

TEST_CASE("TranslateSDLEvent maps SDL window lifecycle events")
{
    const auto requireTranslatedType = [](Uint32 eventType, Life::EventType expectedType)
    {
        SDL_Event event{};
        event.type = eventType;

        Life::Scope<Life::Event> translatedEvent = Life::TranslateSDLEvent(event);
        REQUIRE(translatedEvent != nullptr);
        CHECK(translatedEvent->GetEventType() == expectedType);
    };

    requireTranslatedType(SDL_EVENT_QUIT, Life::WindowCloseEvent::GetStaticType());
    requireTranslatedType(SDL_EVENT_WINDOW_CLOSE_REQUESTED, Life::WindowCloseEvent::GetStaticType());
    requireTranslatedType(SDL_EVENT_WINDOW_FOCUS_GAINED, Life::WindowFocusGainedEvent::GetStaticType());
    requireTranslatedType(SDL_EVENT_WINDOW_FOCUS_LOST, Life::WindowFocusLostEvent::GetStaticType());
    requireTranslatedType(SDL_EVENT_WINDOW_MINIMIZED, Life::WindowMinimizedEvent::GetStaticType());
    requireTranslatedType(SDL_EVENT_WINDOW_RESTORED, Life::WindowRestoredEvent::GetStaticType());
}

TEST_CASE("TranslateSDLEvent preserves SDL window resize and move payloads")
{
    SDL_Event resizeEvent{};
    resizeEvent.type = SDL_EVENT_WINDOW_RESIZED;
    resizeEvent.window.data1 = 1600;
    resizeEvent.window.data2 = 900;

    Life::Scope<Life::Event> translatedResizeEvent = Life::TranslateSDLEvent(resizeEvent);
    REQUIRE(translatedResizeEvent != nullptr);
    REQUIRE(translatedResizeEvent->GetEventType() == Life::WindowResizeEvent::GetStaticType());
    const auto& windowResizeEvent = static_cast<const Life::WindowResizeEvent&>(*translatedResizeEvent);
    CHECK(windowResizeEvent.GetWidth() == 1600);
    CHECK(windowResizeEvent.GetHeight() == 900);

    SDL_Event movedEvent{};
    movedEvent.type = SDL_EVENT_WINDOW_MOVED;
    movedEvent.window.data1 = -25;
    movedEvent.window.data2 = 75;

    Life::Scope<Life::Event> translatedMovedEvent = Life::TranslateSDLEvent(movedEvent);
    REQUIRE(translatedMovedEvent != nullptr);
    REQUIRE(translatedMovedEvent->GetEventType() == Life::WindowMovedEvent::GetStaticType());
    const auto& windowMovedEvent = static_cast<const Life::WindowMovedEvent&>(*translatedMovedEvent);
    CHECK(windowMovedEvent.GetX() == -25);
    CHECK(windowMovedEvent.GetY() == 75);
}

TEST_CASE("TranslateSDLEvent returns null for unsupported SDL events")
{
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;

    CHECK(Life::TranslateSDLEvent(event) == nullptr);
}

TEST_CASE("Platform detection initializes and exposes basic metadata")
{
    Life::Log::Init();

    Life::PlatformDetection::Initialize();
    CHECK(Life::PlatformDetection::IsInitialized());

    const Life::PlatformInfo& platformInfo = Life::PlatformDetection::GetPlatformInfo();
    CHECK_FALSE(platformInfo.platformName.empty());
    CHECK_FALSE(platformInfo.architectureName.empty());
    CHECK_FALSE(platformInfo.compilerName.empty());
    CHECK_FALSE(platformInfo.buildDate.empty());
    CHECK_FALSE(platformInfo.buildTime.empty());
    CHECK_FALSE(platformInfo.buildType.empty());
}

TEST_CASE("Error handling captures engine error details in Result")
{
    Life::PlatformDetection::Initialize();

    Life::Result<int> result = Life::ErrorHandling::Try([]() -> int
    {
        LIFE_THROW_ERROR(Life::ErrorCode::InvalidState, "result failure");
        return 0;
    });

    REQUIRE(result.IsFailure());
    CHECK(result.GetError().GetCode() == Life::ErrorCode::InvalidState);
    CHECK(result.GetError().GetErrorMessage() == "result failure");
    CHECK_FALSE(result.GetError().GetContext().threadId.empty());
    CHECK(result.GetError().GetContext().timestamp > 0);
    CHECK(result.GetError().ToString().find("InvalidState") != std::string::npos);
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

TEST_CASE("Log configuration creates file sink directories")
{
    const Life::LogSpecification originalSpecification = Life::Log::GetSpecification();
    const std::filesystem::path logDirectory = std::filesystem::temp_directory_path() / "LifeTests" / "LoggingDirectoryCreation";
    const std::filesystem::path logFilePath = logDirectory / "life.log";

    std::filesystem::remove_all(logDirectory);

    Life::LogSpecification fileSpecification = originalSpecification;
    fileSpecification.CoreLoggerName = "TEST_FILE_CORE";
    fileSpecification.ClientLoggerName = "TEST_FILE_CLIENT";
    fileSpecification.EnableConsole = false;
    fileSpecification.EnableFile = true;
    fileSpecification.FilePath = logFilePath.string();

    Life::Log::Configure(fileSpecification);

    std::shared_ptr<spdlog::logger> coreLogger = Life::Log::GetCoreLogger();
    REQUIRE(coreLogger != nullptr);
    coreLogger->info("file sink initialization");
    coreLogger->flush();

    CHECK(std::filesystem::exists(logDirectory));
    CHECK(std::filesystem::exists(logFilePath));

    coreLogger.reset();
    Life::Log::Configure(originalSpecification);
    std::filesystem::remove_all(logDirectory);
}

TEST_CASE("Log configuration failure preserves the current logger state")
{
    const Life::LogSpecification originalSpecification = Life::Log::GetSpecification();

    Life::LogSpecification validSpecification = originalSpecification;
    validSpecification.CoreLoggerName = "TEST_STABLE_CORE";
    validSpecification.ClientLoggerName = "TEST_STABLE_CLIENT";
    validSpecification.EnableFile = false;

    Life::Log::Configure(validSpecification);

    Life::LogSpecification invalidSpecification = validSpecification;
    invalidSpecification.EnableFile = true;
    invalidSpecification.FilePath.clear();

    CHECK_THROWS_AS(Life::Log::Configure(invalidSpecification), std::runtime_error);

    const std::shared_ptr<spdlog::logger> coreLogger = Life::Log::GetCoreLogger();
    const std::shared_ptr<spdlog::logger> clientLogger = Life::Log::GetClientLogger();
    const Life::LogSpecification activeSpecification = Life::Log::GetSpecification();

    REQUIRE(coreLogger != nullptr);
    REQUIRE(clientLogger != nullptr);
    CHECK(coreLogger->name() == validSpecification.CoreLoggerName);
    CHECK(clientLogger->name() == validSpecification.ClientLoggerName);
    CHECK(activeSpecification.CoreLoggerName == validSpecification.CoreLoggerName);
    CHECK(activeSpecification.ClientLoggerName == validSpecification.ClientLoggerName);
    CHECK(activeSpecification.EnableFile == validSpecification.EnableFile);

    Life::Log::Configure(originalSpecification);
}

TEST_CASE("Crash diagnostics writes handled exception reports")
{
    const Life::CrashReportingSpecification originalSpecification = Life::CrashDiagnostics::GetSpecification();
    const bool wasInstalled = Life::CrashDiagnostics::IsInstalled();
    const std::filesystem::path reportDirectory = std::filesystem::temp_directory_path() / "LifeTests" / "CrashReports";

    std::error_code initialCleanupError;
    std::filesystem::remove_all(reportDirectory, initialCleanupError);
    REQUIRE(initialCleanupError.value() == 0);
    Life::CrashDiagnostics::Shutdown();

    Life::CrashReportingSpecification specification = originalSpecification;
    specification.Enabled = true;
    specification.InstallHandlers = false;
    specification.WriteReport = true;
    specification.WriteMiniDump = false;
    specification.ReportDirectory = reportDirectory.string();
    specification.MaxStackFrames = 16;

    Life::CrashDiagnostics::Configure(specification);
    Life::CrashDiagnostics::SetApplicationInfo("CrashTestApp", { "CrashTestApp", "--synthetic" });

    std::filesystem::path reportPath;
    try
    {
        throw std::runtime_error("synthetic crash for crash report test");
    }
    catch (const std::exception& exception)
    {
        reportPath = Life::CrashDiagnostics::ReportHandledException(exception, "EngineTests");
    }

    REQUIRE_FALSE(reportPath.empty());
    CHECK(std::filesystem::exists(reportPath));

    std::string reportText;
    {
        std::ifstream reportStream(reportPath);
        REQUIRE(reportStream.is_open());
        std::ostringstream reportContents;
        reportContents << reportStream.rdbuf();
        reportText = reportContents.str();
    }

    CHECK(reportPath.parent_path() == std::filesystem::absolute(reportDirectory));
    CHECK(reportText.find("CrashTestApp") != std::string::npos);
    CHECK(reportText.find("synthetic crash for crash report test") != std::string::npos);
    CHECK(reportText.find("EngineTests") != std::string::npos);
    CHECK(reportText.find("handled-exception") != std::string::npos);

    Life::CrashDiagnostics::Configure(originalSpecification);
    if (wasInstalled)
        Life::CrashDiagnostics::Install();
    else
        Life::CrashDiagnostics::Shutdown();

    std::error_code finalCleanupError;
    std::filesystem::remove_all(reportDirectory, finalCleanupError);
    CHECK(finalCleanupError.value() == 0);
}

TEST_CASE("Application startup preserves init close shutdown ordering")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto* applicationInstance = application.get();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    applicationInstance->GetTestRuntime().QueueEvent<Life::WindowCloseEvent>();

    Life::RunApplication(*host);

    CHECK(applicationInstance->InitCount == 1);
    CHECK(applicationInstance->ShutdownCount == 1);
    CHECK(applicationInstance->UpdateCount == 0);
    CHECK(applicationInstance->GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent", "shutdown" });
}

TEST_CASE("Application Initialize aliases host bound initialization")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());

    applicationInstance.Initialize();

    CHECK(host->IsInitialized());
    CHECK(host->IsRunning());
    CHECK(applicationInstance.InitCount == 1);
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init" });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("Application lifecycle operations are idempotent when host bound")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());

    applicationInstance.Initialize();
    applicationInstance.Initialize();

    CHECK(host->IsInitialized());
    CHECK(host->IsRunning());
    CHECK(applicationInstance.InitCount == 1);

    applicationInstance.Shutdown();
    applicationInstance.Shutdown();

    CHECK_FALSE(host->IsRunning());
    CHECK(applicationInstance.ShutdownCount == 0);

    applicationInstance.Finalize();
    applicationInstance.Finalize();

    CHECK_FALSE(host->IsInitialized());
    CHECK(applicationInstance.ShutdownCount == 1);
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init", "shutdown" });
}

TEST_CASE("Application event propagation order is OnEvent then EventBus then built in shutdown")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());
    host->Initialize();
    applicationInstance.SubscribeEvent<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        applicationInstance.Trace.emplace_back(std::string("event_bus:") + event.GetName());
        return false;
    });

    Life::WindowCloseEvent event;
    host->HandleEvent(event);

    CHECK(event.Handled);
    CHECK_FALSE(host->IsRunning());
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent", "event_bus:WindowCloseEvent" });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("External event pump close event triggers shutdown before update")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());
    host->Initialize();

    std::vector<Life::Scope<Life::Event>> pendingEvents;
    pendingEvents.emplace_back(Life::CreateScope<Life::WindowCloseEvent>());
    auto lastFrameTime = std::chrono::steady_clock::now();

    CHECK_FALSE(Life::RunApplicationLoopIteration(*host, lastFrameTime, pendingEvents, true));
    CHECK(applicationInstance.GetTestRuntime().PollCount == 0);
    CHECK(applicationInstance.UpdateCount == 0);
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent" });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("Handled close events do not trigger built in shutdown")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());
    applicationInstance.HandleCloseInOnEvent = true;
    host->Initialize();

    Life::WindowCloseEvent event;
    host->HandleEvent(event);

    CHECK(event.Handled);
    CHECK(host->IsRunning());
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowCloseEvent" });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("External event pump path delivers queued events without polling runtime")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());
    applicationInstance.ShutdownOnUpdate = true;
    host->Initialize();
    applicationInstance.GetTestRuntime().QueueEvent<Life::WindowCloseEvent>();
    applicationInstance.SubscribeEvent<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& event)
    {
        applicationInstance.Trace.emplace_back(std::string("event_bus:") + event.GetName());
        CHECK(event.GetWidth() == 1600);
        CHECK(event.GetHeight() == 900);
        return false;
    });

    std::vector<Life::Scope<Life::Event>> pendingEvents;
    pendingEvents.emplace_back(Life::CreateScope<Life::WindowResizeEvent>(1600, 900));
    auto lastFrameTime = std::chrono::steady_clock::now();

    CHECK_FALSE(Life::RunApplicationLoopIteration(*host, lastFrameTime, pendingEvents, true));
    CHECK(applicationInstance.GetTestRuntime().PollCount == 0);
    CHECK(applicationInstance.GetTestRuntime().QueuedEvents.size() == 1);
    CHECK(applicationInstance.UpdateCount == 1);
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init", "on_event:WindowResizeEvent", "event_bus:WindowResizeEvent", "update" });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}
