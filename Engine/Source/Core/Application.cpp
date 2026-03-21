#include "Core/Application.h"
#include "Core/Events/ApplicationEvent.h"
#include "Core/Log.h"

#include <chrono>
#include <utility>

namespace Life
{
    Application::Application(ApplicationSpecification specification)
        : m_Specification(std::move(specification)),
          m_Window(CreatePlatformWindow(WindowSpecification
          {
              m_Specification.Name,
              m_Specification.Width,
              m_Specification.Height,
              m_Specification.VSync
          }))
    {
        LOG_CORE_INFO("Initialized application '{}'", m_Specification.Name);
    }

    Application::~Application()
    {
        m_Window.reset();
        LOG_CORE_INFO("Shut down application '{}'", m_Specification.Name);
    }

    void Application::Initialize(bool useExternalEventPump)
    {
        if (m_Initialized)
            return;

        m_UseExternalEventPump = useExternalEventPump;
        m_Running = true;
        m_Initialized = true;

        OnInit();
    }

    void Application::Startup()
    {
        Initialize();

        using clock = std::chrono::steady_clock;
        auto lastFrameTime = clock::now();

        while (m_Running)
        {
            auto currentFrameTime = clock::now();
            float timestep = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
            lastFrameTime = currentFrameTime;

            RunFrame(timestep);
        }

        Finalize();
    }

    void Application::RunFrame(float timestep)
    {
        if (!m_Running)
            return;

        if (!m_UseExternalEventPump)
            ProcessEvents();

        OnUpdate(timestep);
    }

    void Application::HandleEvent(Event& event)
    {
        OnEvent(event);
        m_EventBus.Dispatch(event);

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& closeEvent)
        {
            if (closeEvent.Handled)
                return false;

            Shutdown();
            return true;
        });
    }

    void Application::Shutdown()
    {
        m_Running = false;
    }

    void Application::Finalize()
    {
        if (!m_Initialized)
            return;

        OnShutdown();
        m_Initialized = false;
    }

    void Application::ProcessEvents()
    {
        while (Scope<Event> event = m_Window->PollEvent())
            HandleEvent(*event);
    }
}
