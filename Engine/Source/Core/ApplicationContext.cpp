#include "Core/ApplicationContext.h"

#include <stdexcept>
#include <utility>

namespace Life
{
    void ApplicationContext::Bind(
        Window& window,
        ApplicationRuntime& runtime,
        ApplicationContext::StateBinding stateBinding,
        std::function<void()> initializeCallback,
        std::function<void(float)> runFrameCallback,
        std::function<void()> shutdownCallback,
        std::function<void()> finalizeCallback)
    {
        m_Window = &window;
        m_Runtime = &runtime;
        m_Running = &stateBinding.Running;
        m_Initialized = &stateBinding.Initialized;
        m_InitializeCallback = std::move(initializeCallback);
        m_RunFrameCallback = std::move(runFrameCallback);
        m_ShutdownCallback = std::move(shutdownCallback);
        m_FinalizeCallback = std::move(finalizeCallback);
    }

    bool ApplicationContext::IsBound() const
    {
        return m_Window != nullptr && m_Runtime != nullptr && m_Running != nullptr && m_Initialized != nullptr;
    }

    void ApplicationContext::Initialize()
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        m_InitializeCallback();
    }

    void ApplicationContext::RunFrame(float timestep)
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        m_RunFrameCallback(timestep);
    }

    void ApplicationContext::RequestShutdown()
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        m_ShutdownCallback();
    }

    void ApplicationContext::Finalize()
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        m_FinalizeCallback();
    }

    bool ApplicationContext::IsRunning() const
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        return *m_Running;
    }

    bool ApplicationContext::IsInitialized() const
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        return *m_Initialized;
    }

    ApplicationRuntime& ApplicationContext::GetRuntime()
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        return *m_Runtime;
    }

    const ApplicationRuntime& ApplicationContext::GetRuntime() const
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        return *m_Runtime;
    }

    Window& ApplicationContext::GetWindow()
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        return *m_Window;
    }

    const Window& ApplicationContext::GetWindow() const
    {
        if (!IsBound())
            throw std::logic_error("ApplicationContext is not bound.");

        return *m_Window;
    }
}
