#pragma once

#include <doctest/doctest.h>

#include "Core/ApplicationHost.h"
#include "Core/ApplicationRunner.h"
#include "Engine.h"
#include "Platform/SDL/SDLEvent.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Life::Tests
{
    class TestWindow final : public Life::Window
    {
    public:
        explicit TestWindow(Life::WindowSpecification specification)
            : m_Specification(std::move(specification))
        {
        }

        const Life::WindowSpecification& GetSpecification() const override
        {
            return m_Specification;
        }

        void* GetNativeHandle() const override
        {
            return nullptr;
        }

    private:
        Life::WindowSpecification m_Specification;
    };

    class TestRuntime final : public Life::ApplicationRuntime
    {
    public:
        Life::Scope<Life::Window> CreatePlatformWindow(const Life::WindowSpecification& specification) override
        {
            ++CreateWindowCount;
            return Life::CreateScope<TestWindow>(specification);
        }

        Life::Scope<Life::Event> PollEvent() override
        {
            ++PollCount;

            if (QueuedEvents.empty())
                return nullptr;

            Life::Scope<Life::Event> event = std::move(QueuedEvents.front());
            QueuedEvents.erase(QueuedEvents.begin());
            return event;
        }

        template<typename TEvent, typename... TArguments>
        void QueueEvent(TArguments&&... arguments)
        {
            QueuedEvents.emplace_back(Life::CreateScope<TEvent>(std::forward<TArguments>(arguments)...));
        }

        int CreateWindowCount = 0;
        int PollCount = 0;
        std::vector<Life::Scope<Life::Event>> QueuedEvents;
    };

    class FakeGraphicsDevice final : public Life::GraphicsDevice
    {
    public:
        bool BeginFrame() override
        {
            ++BeginFrameCallCount;
            FrameActive = true;
            return BeginFrameResult;
        }

        void Present() override
        {
            ++PresentCallCount;
            FrameActive = false;
        }

        nvrhi::ITexture* GetCurrentBackBuffer() override
        {
            return CurrentBackBuffer;
        }

        nvrhi::IDevice* GetNvrhiDevice() override
        {
            return Device;
        }

        nvrhi::ICommandList* GetCurrentCommandList() override
        {
            return FrameActive ? CommandList : nullptr;
        }

        uint32_t GetBackBufferWidth() const override
        {
            return BackBufferWidth;
        }

        uint32_t GetBackBufferHeight() const override
        {
            return BackBufferHeight;
        }

        Life::GraphicsBackend GetBackend() const override
        {
            return Backend;
        }

        void Resize(uint32_t width, uint32_t height) override
        {
            ++ResizeCallCount;
            LastResizeWidth = width;
            LastResizeHeight = height;
            BackBufferWidth = width;
            BackBufferHeight = height;
        }

        Life::GraphicsBackend Backend = Life::GraphicsBackend::None;
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ITexture* CurrentBackBuffer = nullptr;
        nvrhi::ICommandList* CommandList = nullptr;
        uint32_t BackBufferWidth = 0;
        uint32_t BackBufferHeight = 0;
        uint32_t LastResizeWidth = 0;
        uint32_t LastResizeHeight = 0;
        int BeginFrameCallCount = 0;
        int PresentCallCount = 0;
        int ResizeCallCount = 0;
        bool BeginFrameResult = false;
        bool FrameActive = false;
    };

    class TestApplication final : public Life::Application
    {
    public:
        TestApplication()
            : Life::Application(CreateSpecification())
        {
        }

        static Life::ApplicationSpecification CreateSpecification()
        {
            Life::ApplicationSpecification specification;
            specification.Name = "Test Application";
            specification.Width = 640;
            specification.Height = 480;
            specification.VSync = false;
            specification.Concurrency.JobWorkerCount = 1;
            specification.Concurrency.AsyncWorkerCount = 1;
            return specification;
        }

        TestRuntime& GetTestRuntime()
        {
            return static_cast<TestRuntime&>(GetService<Life::ApplicationRuntime>());
        }

        const std::vector<std::string>& GetTrace() const
        {
            return Trace;
        }

        int InitCount = 0;
        int ShutdownCount = 0;
        int UpdateCount = 0;
        bool HandleCloseInOnEvent = false;
        bool ShutdownOnUpdate = false;
        std::vector<std::string> Trace;

    protected:
        void OnInit() override
        {
            ++InitCount;
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
            ++UpdateCount;
            Trace.emplace_back("update");

            if (ShutdownOnUpdate)
                RequestShutdown();
        }

        void OnEvent(Life::Event& event) override
        {
            Trace.emplace_back(std::string("on_event:") + event.GetName());

            if (HandleCloseInOnEvent && event.GetEventType() == Life::WindowCloseEvent::GetStaticType())
                event.Accept();
        }
    };

    struct TestService
    {
        int Value = 0;
    };

    class ConfigurableTestApplication final : public Life::Application
    {
    public:
        explicit ConfigurableTestApplication(Life::ApplicationSpecification specification)
            : Life::Application(std::move(specification))
        {
        }
    };

    class ThrowingLifecycleApplication final : public Life::Application
    {
    public:
        ThrowingLifecycleApplication()
            : Life::Application(TestApplication::CreateSpecification())
        {
        }

        int InitCount = 0;
        int ShutdownCount = 0;
        int UpdateCount = 0;
        bool ThrowOnInit = false;
        bool ThrowOnShutdown = false;
        bool ThrowOnUpdate = false;

    protected:
        void OnInit() override
        {
            ++InitCount;
            if (ThrowOnInit)
                throw std::runtime_error("synthetic init failure");
        }

        void OnShutdown() override
        {
            ++ShutdownCount;
            if (ThrowOnShutdown)
                throw std::runtime_error("synthetic shutdown failure");
        }

        void OnUpdate(float timestep) override
        {
            (void)timestep;
            ++UpdateCount;
            if (ThrowOnUpdate)
                throw std::runtime_error("synthetic update failure");
        }
    };

    class ThrowingRuntime final : public Life::ApplicationRuntime
    {
    public:
        explicit ThrowingRuntime(std::string message)
            : m_Message(std::move(message))
        {
        }

        Life::Scope<Life::Window> CreatePlatformWindow(const Life::WindowSpecification& specification) override
        {
            (void)specification;
            throw std::runtime_error(m_Message);
        }

        Life::Scope<Life::Event> PollEvent() override
        {
            return nullptr;
        }

    private:
        std::string m_Message;
    };

    struct CrashDiagnosticsConfigurationRestorer final
    {
        CrashDiagnosticsConfigurationRestorer()
            : WasInstalled(Life::CrashDiagnostics::IsInstalled())
            , OriginalSpecification(Life::CrashDiagnostics::GetSpecification())
        {
        }

        ~CrashDiagnosticsConfigurationRestorer()
        {
            Life::CrashDiagnostics::Configure(OriginalSpecification);
            if (WasInstalled)
                Life::CrashDiagnostics::Install();
            else
                Life::CrashDiagnostics::Shutdown();
        }

        bool WasInstalled = false;
        Life::CrashReportingSpecification OriginalSpecification;
    };

    inline bool CrashReportingSpecificationsEqual(
        const Life::CrashReportingSpecification& left,
        const Life::CrashReportingSpecification& right)
    {
        return left.Enabled == right.Enabled
            && left.InstallHandlers == right.InstallHandlers
            && left.CaptureSignals == right.CaptureSignals
            && left.CaptureTerminate == right.CaptureTerminate
            && left.CaptureUnhandledExceptions == right.CaptureUnhandledExceptions
            && left.WriteReport == right.WriteReport
            && left.WriteMiniDump == right.WriteMiniDump
            && left.ReportDirectory == right.ReportDirectory
            && left.MaxStackFrames == right.MaxStackFrames;
    }

    struct CrashDiagnosticsTestScope final
    {
        explicit CrashDiagnosticsTestScope(std::filesystem::path reportDirectory)
            : WasInstalled(Life::CrashDiagnostics::IsInstalled())
            , OriginalSpecification(Life::CrashDiagnostics::GetSpecification())
            , ReportDirectory(std::move(reportDirectory))
        {
            std::error_code cleanupError;
            std::filesystem::remove_all(ReportDirectory, cleanupError);
            Life::CrashDiagnostics::Shutdown();

            Life::CrashReportingSpecification specification = OriginalSpecification;
            specification.Enabled = true;
            specification.InstallHandlers = false;
            specification.WriteReport = true;
            specification.WriteMiniDump = false;
            specification.ReportDirectory = ReportDirectory.string();
            specification.MaxStackFrames = 16;
            Life::CrashDiagnostics::Configure(specification);
        }

        ~CrashDiagnosticsTestScope()
        {
            Life::CrashDiagnostics::Configure(OriginalSpecification);
            if (WasInstalled)
                Life::CrashDiagnostics::Install();
            else
                Life::CrashDiagnostics::Shutdown();

            std::error_code cleanupError;
            std::filesystem::remove_all(ReportDirectory, cleanupError);
        }

        bool WasInstalled = false;
        Life::CrashReportingSpecification OriginalSpecification;
        std::filesystem::path ReportDirectory;
    };

    inline std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        if (!stream.is_open())
            throw std::runtime_error("Failed to open file: " + path.string());

        std::ostringstream contents;
        contents << stream.rdbuf();
        return contents.str();
    }

    template<typename TPredicate>
    bool WaitForCondition(TPredicate&& predicate, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
                return true;
            std::this_thread::yield();
        }

        return predicate();
    }
}
