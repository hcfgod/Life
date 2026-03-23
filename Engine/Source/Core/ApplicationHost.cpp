#include "Core/ApplicationHost.h"
#include "Platform/Platform.h"

#include <stdexcept>
#include <utility>

namespace Life
{
    ApplicationHost::ApplicationHost(Scope<Application> application)
        : ApplicationHost(std::move(application), CreatePlatformApplicationRuntime())
    {
    }

    ApplicationHost::ApplicationHost(Scope<Application> application, Scope<ApplicationRuntime> runtime)
        : m_Application(std::move(application)),
          m_Runtime(std::move(runtime))
    {
        if (m_Application == nullptr)
            throw std::logic_error("ApplicationHost requires a valid application instance.");

        if (m_Runtime == nullptr)
            throw std::logic_error("ApplicationHost requires a valid application runtime.");

        const ApplicationSpecification& specification = m_Application->GetSpecification();
        Log::Configure(specification.Logging);
        PlatformDetection::Initialize();
        LOG_CORE_INFO("Constructed application '{}'", specification.Name);
        m_Window = m_Runtime->CreatePlatformWindow(WindowSpecification
        {
            specification.Name,
            specification.Width,
            specification.Height,
            specification.VSync
        });

        m_Context.Bind(
            *m_Window,
            *m_Runtime,
            ApplicationContext::StateBinding{ m_Running, m_Initialized },
            [this]() { Initialize(); },
            [this](float timestep) { RunFrame(timestep); },
            [this]() { Shutdown(); },
            [this]() { Finalize(); });

        m_Application->BindHost(m_Context, m_EventRouter);
    }

    ApplicationHost::~ApplicationHost()
    {
        Finalize();
        m_Window.reset();
        m_Runtime.reset();
        m_Application.reset();
    }

    void ApplicationHost::Initialize()
    {
        if (m_Initialized)
            return;

        m_Running = true;
        m_Initialized = true;
        m_Application->OnHostInitialize();
    }

    void ApplicationHost::RunFrame(float timestep)
    {
        if (!m_Running)
            return;

        m_Application->OnHostRunFrame(timestep);
    }

    void ApplicationHost::HandleEvent(Event& event)
    {
        m_EventRouter.Route(*m_Application, event);
    }

    void ApplicationHost::Shutdown()
    {
        m_Running = false;
    }

    void ApplicationHost::Finalize()
    {
        if (!m_Initialized)
            return;

        m_Application->OnHostFinalize();
        m_Running = false;
        m_Initialized = false;
    }
}
