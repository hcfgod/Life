#include "TestSupport.h"

using namespace Life::Tests;

TEST_CASE("CreateScope stores values")
{
    Life::Scope<int> value = Life::CreateScope<int>(42);

    CHECK(*value == 42);
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
