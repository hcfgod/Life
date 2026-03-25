#include "TestSupport.h"

using namespace Life::Tests;

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
    CHECK(event.IsHandled());
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

TEST_CASE("EventBus honors subscription priorities and filters")
{
    Life::EventBus bus;
    std::vector<std::string> callOrder;

    Life::EventSubscriptionOptions<Life::WindowResizeEvent> lowPriorityOptions;
    lowPriorityOptions.Priority = -10;
    bus.Subscribe<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& event)
    {
        callOrder.emplace_back("low");
        CHECK(event.GetWidth() == 1600);
        return false;
    }, std::move(lowPriorityOptions));

    Life::EventSubscriptionOptions<Life::WindowResizeEvent> filteredOptions;
    filteredOptions.Priority = 50;
    filteredOptions.Filter = [](const Life::WindowResizeEvent& event)
    {
        return event.GetWidth() == 1920;
    };
    bus.Subscribe<Life::WindowResizeEvent>([&](Life::WindowResizeEvent&)
    {
        callOrder.emplace_back("filtered");
        return false;
    }, std::move(filteredOptions));

    Life::EventSubscriptionOptions<Life::WindowResizeEvent> highPriorityOptions;
    highPriorityOptions.Priority = 10;
    bus.Subscribe<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& event)
    {
        callOrder.emplace_back("high");
        CHECK(event.GetHeight() == 900);
        return false;
    }, std::move(highPriorityOptions));

    CHECK_FALSE(bus.Dispatch<Life::WindowResizeEvent>(1600, 900));
    CHECK(callOrder == std::vector<std::string>{ "high", "low" });
}

TEST_CASE("EventBus keeps handled and propagation state separate")
{
    Life::EventBus bus;
    int callCount = 0;

    bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        ++callCount;
        CHECK_FALSE(event.IsHandled());
        CHECK_FALSE(event.IsPropagationStopped());
        return Life::EventDispatchResult::Handled;
    });

    bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        ++callCount;
        CHECK(event.IsHandled());
        CHECK_FALSE(event.IsPropagationStopped());
        return Life::EventDispatchResult::StopPropagation;
    });

    bus.Subscribe<Life::WindowCloseEvent>([&](Life::WindowCloseEvent&)
    {
        ++callCount;
        return false;
    });

    Life::WindowCloseEvent event;
    bus.Dispatch(event);

    CHECK(callCount == 2);
    CHECK(event.IsHandled());
    CHECK(event.IsPropagationStopped());
}

TEST_CASE("EventBus enforces owner thread affinity")
{
    Life::EventBus bus;
    std::atomic<bool> threw = false;

    CHECK(bus.GetThreadingModel() == Life::EventBusThreadingModel::OwnerThreadOnly);
    CHECK(bus.IsOwnerThread());

    std::thread worker([&]()
    {
        try
        {
            bus.Dispatch<Life::WindowCloseEvent>();
        }
        catch (const std::logic_error&)
        {
            threw.store(true, std::memory_order_relaxed);
        }
    });

    worker.join();

    CHECK(threw.load(std::memory_order_relaxed));
}

TEST_CASE("WindowResizeEvent stores its dimensions")
{
    Life::WindowResizeEvent event(1600, 900);

    CHECK(event.GetWidth() == 1600);
    CHECK(event.GetHeight() == 900);
    CHECK(event.IsInCategory(Life::EventCategory::Window));
    CHECK(event.IsInCategory(Life::EventCategory::Application));
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

    CHECK(event.IsHandled());
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

    CHECK(event.IsHandled());
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
