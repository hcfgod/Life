#include "TestSupport.h"

using namespace Life::Tests;

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
    CHECK(&host->GetServices().Get<Life::ApplicationContext>() == &host->GetContext());
    CHECK(&host->GetServices().Get<Life::JobSystem>() == &Life::GetJobSystem());
    CHECK(&host->GetServices().Get<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    CHECK(&host->GetServices().Get<Life::ApplicationRuntime>() == &host->GetRuntime());
    CHECK(&applicationInstance->GetService<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    CHECK(&applicationInstance->GetService<Life::ApplicationHost>() == host.get());
    CHECK(&applicationInstance->GetService<Life::Window>() == &host->GetWindow());

    TestService service{ 7 };
    host->GetServices().Register<TestService>(service);

    CHECK(host->GetServices().Has<TestService>());
    CHECK(applicationInstance->HasService<TestService>());
    CHECK(applicationInstance->TryGetService<TestService>() == &service);
    CHECK(applicationInstance->GetService<TestService>().Value == 7);
}

TEST_CASE("Global service registry falls back after host destruction")
{
    {
        auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(&Life::GetServices() == &host->GetServices());
    }

    CHECK_FALSE(Life::GetServices().Has<Life::ApplicationHost>());
}

TEST_CASE("Creating a second ApplicationHost while one is live is rejected")
{
    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    CHECK(&Life::GetServices() == &host->GetServices());

    try
    {
        [[maybe_unused]] auto secondHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        FAIL("Expected second host creation to throw");
    }
    catch (const Life::Error& error)
    {
        CHECK(error.GetCode() == Life::ErrorCode::InvalidState);
    }

    CHECK(&Life::GetServices() == &host->GetServices());

    host.reset();
    CHECK_FALSE(Life::GetServices().Has<Life::ApplicationHost>());
}
