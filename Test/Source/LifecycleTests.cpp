#include "TestSupport.h"

#include <type_traits>

using namespace Life::Tests;

namespace
{
    template<typename T, typename = void>
    inline constexpr bool HasGetContextMember = false;

    template<typename T>
    inline constexpr bool HasGetContextMember<T, std::void_t<decltype(std::declval<T&>().GetContext())>> = true;

    template<typename T, typename = void>
    inline constexpr bool HasGetServicesMember = false;

    template<typename T>
    inline constexpr bool HasGetServicesMember<T, std::void_t<decltype(std::declval<T&>().GetServices())>> = true;

    template<typename T, typename = void>
    inline constexpr bool HasInitializeMember = false;

    template<typename T>
    inline constexpr bool HasInitializeMember<T, std::void_t<decltype(std::declval<T&>().Initialize())>> = true;

    template<typename T, typename = void>
    inline constexpr bool HasRunFrameMember = false;

    template<typename T>
    inline constexpr bool HasRunFrameMember<T, std::void_t<decltype(std::declval<T&>().RunFrame(0.016f))>> = true;

    template<typename T, typename = void>
    inline constexpr bool HasFinalizeMember = false;

    template<typename T>
    inline constexpr bool HasFinalizeMember<T, std::void_t<decltype(std::declval<T&>().Finalize())>> = true;

    template<typename T, typename = void>
    inline constexpr bool HasRequestShutdownMember = false;

    template<typename T>
    inline constexpr bool HasRequestShutdownMember<T, std::void_t<decltype(std::declval<T&>().RequestShutdown())>> = true;
}

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
    CHECK_THROWS_AS(application.HandleEvent(event), std::logic_error);
    CHECK_THROWS_AS(application.RequestShutdown(), std::logic_error);
    CHECK_THROWS_AS(application.GetWindow(), std::logic_error);
    CHECK_THROWS_AS(application.GetService<TestService>(), std::logic_error);
    CHECK_THROWS_AS(application.SubscribeEvent<Life::WindowCloseEvent>([](Life::WindowCloseEvent&) { return false; }), std::logic_error);
    CHECK_THROWS_AS(application.UnsubscribeEvent(1), std::logic_error);
}

TEST_CASE("Application public surface excludes raw context, registry, and lifecycle control access")
{
    static_assert(!HasGetContextMember<Life::Application>);
    static_assert(!HasGetServicesMember<Life::Application>);
    static_assert(!HasInitializeMember<Life::Application>);
    static_assert(!HasRunFrameMember<Life::Application>);
    static_assert(!HasFinalizeMember<Life::Application>);
    static_assert(HasRequestShutdownMember<Life::Application>);

    CHECK(true);
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

TEST_CASE("Application request shutdown aliases host bound shutdown")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());

    host->Initialize();

    CHECK(host->IsInitialized());
    CHECK(host->IsRunning());
    CHECK(applicationInstance.InitCount == 1);
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init" });

    applicationInstance.RequestShutdown();

    CHECK_FALSE(host->IsRunning());
    CHECK(host->IsInitialized());
    CHECK(applicationInstance.ShutdownCount == 0);

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("ApplicationHost lifecycle operations are idempotent when host bound")
{
    Life::Log::Init();

    auto application = Life::CreateScope<TestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<TestApplication&>(host->GetApplication());

    host->Initialize();
    host->Initialize();

    CHECK(host->IsInitialized());
    CHECK(host->IsRunning());
    CHECK(applicationInstance.InitCount == 1);

    applicationInstance.RequestShutdown();
    applicationInstance.RequestShutdown();

    CHECK_FALSE(host->IsRunning());
    CHECK(applicationInstance.ShutdownCount == 0);

    host->Finalize();
    host->Finalize();

    CHECK_FALSE(host->IsInitialized());
    CHECK(applicationInstance.ShutdownCount == 1);
    CHECK(applicationInstance.GetTrace() == std::vector<std::string>{ "init", "shutdown" });
}
