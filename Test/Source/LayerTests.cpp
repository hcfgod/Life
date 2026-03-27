#include "TestSupport.h"

using namespace Life::Tests;

namespace
{
    class LayerTestApplication final : public Life::Application
    {
    public:
        LayerTestApplication()
            : Life::Application(TestApplication::CreateSpecification())
        {
        }

        std::vector<std::string> Trace;
        int ShutdownCount = 0;

    protected:
        void OnInit() override
        {
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
            Trace.emplace_back("update");
        }

        void OnEvent(Life::Event& event) override
        {
            Trace.emplace_back(std::string("on_event:") + event.GetName());
        }
    };

    class TracingLayer final : public Life::Layer
    {
    public:
        TracingLayer(std::string debugName, std::vector<std::string>& trace)
            : Life::Layer(std::move(debugName))
            , m_Trace(trace)
        {
        }

        bool StopWindowClosePropagation = false;

    protected:
        void OnAttach() override
        {
            CHECK(IsAttached());
            CHECK(&GetApplication() != nullptr);
            CHECK(GetWindow().GetSpecification().Width == 640);
            m_Trace.emplace_back(GetDebugName() + ":attach");
        }

        void OnDetach() override
        {
            CHECK(IsAttached());
            m_Trace.emplace_back(GetDebugName() + ":detach");
        }

        void OnUpdate(float timestep) override
        {
            (void)timestep;
            m_Trace.emplace_back(GetDebugName() + ":update");
        }

        void OnEvent(Life::Event& event) override
        {
            m_Trace.emplace_back(GetDebugName() + ":event:" + event.GetName());
            if (StopWindowClosePropagation && event.GetEventType() == Life::WindowCloseEvent::GetStaticType())
                event.StopPropagation();
        }

    private:
        std::vector<std::string>& m_Trace;
    };
}

TEST_CASE("Application layer event propagation order is application then overlays then layers then event bus")
{
    Life::Log::Init();

    auto application = Life::CreateScope<LayerTestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<LayerTestApplication&>(host->GetApplication());
    host->Initialize();

    Life::LayerRef gameplayLayer = Life::CreateRef<TracingLayer>("GameplayLayer", applicationInstance.Trace);
    Life::LayerRef overlayLayer = Life::CreateRef<TracingLayer>("EditorOverlay", applicationInstance.Trace);
    applicationInstance.PushLayer(gameplayLayer);
    applicationInstance.PushOverlay(overlayLayer);
    applicationInstance.SubscribeEvent<Life::WindowResizeEvent>([&](Life::WindowResizeEvent& event)
    {
        applicationInstance.Trace.emplace_back(std::string("event_bus:") + event.GetName());
        CHECK(event.GetWidth() == 1600);
        CHECK(event.GetHeight() == 900);
        return false;
    });

    Life::WindowResizeEvent event(1600, 900);
    host->HandleEvent(event);

    CHECK(applicationInstance.Trace == std::vector<std::string>{
        "init",
        "GameplayLayer:attach",
        "EditorOverlay:attach",
        "on_event:WindowResizeEvent",
        "EditorOverlay:event:WindowResizeEvent",
        "GameplayLayer:event:WindowResizeEvent",
        "event_bus:WindowResizeEvent"
    });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("ApplicationHost frame execution updates application before regular layers and overlays")
{
    Life::Log::Init();

    auto application = Life::CreateScope<LayerTestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<LayerTestApplication&>(host->GetApplication());
    host->Initialize();

    applicationInstance.PushLayer(Life::CreateRef<TracingLayer>("GameplayLayer", applicationInstance.Trace));
    applicationInstance.PushOverlay(Life::CreateRef<TracingLayer>("EditorOverlay", applicationInstance.Trace));
    applicationInstance.Trace.clear();

    host->RunFrame(0.016f);

    CHECK(applicationInstance.Trace == std::vector<std::string>{
        "update",
        "GameplayLayer:update",
        "EditorOverlay:update"
    });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("Layer stop propagation prevents lower layers event bus and built in close handling")
{
    Life::Log::Init();

    auto application = Life::CreateScope<LayerTestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<LayerTestApplication&>(host->GetApplication());
    host->Initialize();

    applicationInstance.PushLayer(Life::CreateRef<TracingLayer>("GameplayLayer", applicationInstance.Trace));
    Life::Ref<TracingLayer> overlayLayer = Life::CreateRef<TracingLayer>("EditorOverlay", applicationInstance.Trace);
    overlayLayer->StopWindowClosePropagation = true;
    applicationInstance.PushOverlay(overlayLayer);
    applicationInstance.SubscribeEvent<Life::WindowCloseEvent>([&](Life::WindowCloseEvent& event)
    {
        applicationInstance.Trace.emplace_back(std::string("event_bus:") + event.GetName());
        return false;
    });

    Life::WindowCloseEvent event;
    host->HandleEvent(event);

    CHECK_FALSE(event.IsHandled());
    CHECK(event.IsPropagationStopped());
    CHECK(host->IsRunning());
    CHECK(applicationInstance.Trace == std::vector<std::string>{
        "init",
        "GameplayLayer:attach",
        "EditorOverlay:attach",
        "on_event:WindowCloseEvent",
        "EditorOverlay:event:WindowCloseEvent"
    });

    host->Finalize();
    CHECK(applicationInstance.ShutdownCount == 1);
}

TEST_CASE("ApplicationHost finalization detaches layers in reverse order after application shutdown")
{
    Life::Log::Init();

    auto application = Life::CreateScope<LayerTestApplication>();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());
    auto& applicationInstance = static_cast<LayerTestApplication&>(host->GetApplication());
    host->Initialize();

    Life::LayerRef gameplayLayer = Life::CreateRef<TracingLayer>("GameplayLayer", applicationInstance.Trace);
    Life::LayerRef overlayLayer = Life::CreateRef<TracingLayer>("EditorOverlay", applicationInstance.Trace);
    applicationInstance.PushLayer(gameplayLayer);
    applicationInstance.PushOverlay(overlayLayer);
    applicationInstance.Trace.clear();

    host->Finalize();

    CHECK(applicationInstance.ShutdownCount == 1);
    CHECK_FALSE(gameplayLayer->IsAttached());
    CHECK_FALSE(overlayLayer->IsAttached());
    CHECK(applicationInstance.Trace == std::vector<std::string>{
        "shutdown",
        "EditorOverlay:detach",
        "GameplayLayer:detach"
    });
}
