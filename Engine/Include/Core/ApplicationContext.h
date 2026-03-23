#pragma once

#include "Core/ApplicationRuntime.h"

#include <functional>

namespace Life
{
    class ApplicationContext
    {
    public:
        ApplicationContext() = default;

        void Bind(
            Window& window,
            ApplicationRuntime& runtime,
            bool& running,
            bool& initialized,
            std::function<void()> initializeCallback,
            std::function<void(float)> runFrameCallback,
            std::function<void()> shutdownCallback,
            std::function<void()> finalizeCallback);

        bool IsBound() const;

        void Initialize();
        void RunFrame(float timestep);
        void RequestShutdown();
        void Finalize();

        bool IsRunning() const;
        bool IsInitialized() const;

        ApplicationRuntime& GetRuntime();
        const ApplicationRuntime& GetRuntime() const;
        Window& GetWindow();
        const Window& GetWindow() const;

    private:
        Window* m_Window = nullptr;
        ApplicationRuntime* m_Runtime = nullptr;
        bool* m_Running = nullptr;
        bool* m_Initialized = nullptr;
        std::function<void()> m_InitializeCallback;
        std::function<void(float)> m_RunFrameCallback;
        std::function<void()> m_ShutdownCallback;
        std::function<void()> m_FinalizeCallback;
    };
}
