#include "Core/Layer.h"

#include "Core/Application.h"

#include <stdexcept>
#include <utility>

namespace Life
{
    Layer::Layer(std::string debugName)
        : m_DebugName(std::move(debugName))
    {
    }

    Application& Layer::GetApplication()
    {
        return RequireApplication();
    }

    const Application& Layer::GetApplication() const
    {
        return RequireApplication();
    }

    Window& Layer::GetWindow()
    {
        return RequireApplication().GetWindow();
    }

    const Window& Layer::GetWindow() const
    {
        return RequireApplication().GetWindow();
    }

    void Layer::Bind(Application& application, bool overlay) noexcept
    {
        m_Application = &application;
        m_Overlay = overlay;
        m_Attached = true;
    }

    void Layer::Unbind() noexcept
    {
        m_Attached = false;
        m_Overlay = false;
        m_Application = nullptr;
    }

    Application& Layer::RequireApplication()
    {
        if (m_Application == nullptr)
            throw std::logic_error("Layer is not attached to an Application.");

        return *m_Application;
    }

    const Application& Layer::RequireApplication() const
    {
        if (m_Application == nullptr)
            throw std::logic_error("Layer is not attached to an Application.");

        return *m_Application;
    }
}
