#pragma once

#include "Core/Memory.h"

#include <string>
#include <utility>

namespace Life
{
    class Application;
    class Event;
    class LayerStack;
    class Window;

    class Layer
    {
    public:
        explicit Layer(std::string debugName = "Layer");
        virtual ~Layer() = default;

        const std::string& GetDebugName() const noexcept { return m_DebugName; }
        bool IsEnabled() const noexcept { return m_Enabled; }
        void SetEnabled(bool enabled) noexcept { m_Enabled = enabled; }
        bool IsAttached() const noexcept { return m_Attached; }
        bool IsOverlay() const noexcept { return m_Overlay; }

        Application& GetApplication();
        const Application& GetApplication() const;
        Window& GetWindow();
        const Window& GetWindow() const;

    protected:
        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(float timestep) {}
        virtual void OnRender() {}
        virtual void OnEvent(Event& event) {}

    private:
        friend class LayerStack;

        void Bind(Application& application, bool overlay) noexcept;
        void Unbind() noexcept;
        Application& RequireApplication();
        const Application& RequireApplication() const;

        std::string m_DebugName;
        Application* m_Application = nullptr;
        bool m_Enabled = true;
        bool m_Attached = false;
        bool m_Overlay = false;
    };

    using LayerRef = Ref<Layer>;
}
