#include "Core/Application.h"
#include "Core/ApplicationRunner.h"
#include "Core/Events/ApplicationEvent.h"
#include "Core/Log.h"

#include <cstdio>
#include <utility>

namespace Life
{
    Application::Application(ApplicationSpecification specification)
        : Application(std::move(specification), CreatePlatformApplicationRuntime())
    {
    }

    Application::Application(ApplicationSpecification specification, Scope<ApplicationRuntime> runtime)
        : m_Specification(std::move(specification)),
          m_Runtime(std::move(runtime))
    {
        Log::Configure(m_Specification.Logging);
        m_Window = m_Runtime->CreatePlatformWindow(WindowSpecification
        {
            m_Specification.Name,
            m_Specification.Width,
            m_Specification.Height,
            m_Specification.VSync
        });
        LOG_CORE_INFO("Initialized application '{}'", m_Specification.Name);
    }

    Application::~Application()
    {
        m_Window.reset();
        m_Runtime.reset();

        try
        {
            LOG_CORE_INFO("Shut down application '{}'", m_Specification.Name);
        }
        catch (...)
        {
            std::fprintf(stderr, "Shut down application '%s'\n", m_Specification.Name.c_str());
        }
    }

    void Application::Initialize()
    {
        if (m_Initialized)
            return;

        m_Running = true;
        m_Initialized = true;

        OnInit();
    }

    void Application::Startup()
    {
        RunApplication(*this);
    }

    void Application::RunFrame(float timestep)
    {
        if (!m_Running)
            return;

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
        m_Running = false;
        m_Initialized = false;
    }
}
