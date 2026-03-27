#include "Core/Application.h"
#include "Core/LayerStack.h"
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

    void Application::HandleEvent(Event& event)
    {
        RequireEventRouter().Route(*this, event);
    }

    void Application::RequestShutdown()
    {
        RequireContext().RequestShutdown();
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

    void Application::PushLayer(Ref<Layer> layer)
    {
        GetLayerStack().PushLayer(std::move(layer));
    }

    void Application::PushOverlay(Ref<Layer> overlay)
    {
        GetLayerStack().PushOverlay(std::move(overlay));
    }

    bool Application::PopLayer(const Ref<Layer>& layer)
    {
        return GetLayerStack().PopLayer(layer);
    }

    bool Application::PopOverlay(const Ref<Layer>& overlay)
    {
        return GetLayerStack().PopOverlay(overlay);
    }

    LayerStack& Application::GetLayerStack()
    {
        return RequireContext().GetService<LayerStack>();
    }

    const LayerStack& Application::GetLayerStack() const
    {
        return RequireContext().GetService<LayerStack>();
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
