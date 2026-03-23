#include "Core/Application.h"

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace Life
{
    Application::Application(ApplicationSpecification specification)
        : m_Specification(std::move(specification))
    {
    }

    Application::~Application()
    {
        try
        {
            LOG_CORE_INFO("Destroyed application '{}'", m_Specification.Name);
        }
        catch (...)
        {
            std::fprintf(stderr, "Destroyed application '%s'\n", m_Specification.Name.c_str());
        }
    }

    void Application::Initialize()
    {
        RequireContext().Initialize();
    }

    void Application::RunFrame(float timestep)
    {
        RequireContext().RunFrame(timestep);
    }

    void Application::HandleEvent(Event& event)
    {
        RequireEventRouter().Route(*this, event);
    }

    void Application::Shutdown()
    {
        RequireContext().RequestShutdown();
    }

    void Application::Finalize()
    {
        RequireContext().Finalize();
    }

    bool Application::IsRunning() const
    {
        return m_Context != nullptr && m_Context->IsRunning();
    }

    bool Application::IsInitialized() const
    {
        return m_Context != nullptr && m_Context->IsInitialized();
    }

    Window& Application::GetWindow()
    {
        return RequireContext().GetWindow();
    }

    const Window& Application::GetWindow() const
    {
        return RequireContext().GetWindow();
    }

    ApplicationContext& Application::GetContext()
    {
        return RequireContext();
    }

    const ApplicationContext& Application::GetContext() const
    {
        return RequireContext();
    }

    void Application::BindHost(ApplicationContext& context, ApplicationEventRouter& eventRouter)
    {
        m_Context = &context;
        m_EventRouter = &eventRouter;
    }

    ApplicationContext& Application::RequireContext()
    {
        if (m_Context == nullptr)
            throw std::logic_error("Application is not bound to an ApplicationHost.");

        return *m_Context;
    }

    const ApplicationContext& Application::RequireContext() const
    {
        if (m_Context == nullptr)
            throw std::logic_error("Application is not bound to an ApplicationHost.");

        return *m_Context;
    }

    ApplicationEventRouter& Application::RequireEventRouter()
    {
        if (m_EventRouter == nullptr)
            throw std::logic_error("Application is not bound to an ApplicationEventRouter.");

        return *m_EventRouter;
    }

    const ApplicationEventRouter& Application::RequireEventRouter() const
    {
        if (m_EventRouter == nullptr)
            throw std::logic_error("Application is not bound to an ApplicationEventRouter.");

        return *m_EventRouter;
    }

    void Application::OnHostInitialize()
    {
        OnInit();
    }

    void Application::OnHostRunFrame(float timestep)
    {
        OnUpdate(timestep);
    }

    void Application::OnHostFinalize()
    {
        OnShutdown();
    }
}
