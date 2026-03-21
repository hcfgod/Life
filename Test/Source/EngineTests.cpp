#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "Engine.h"

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
