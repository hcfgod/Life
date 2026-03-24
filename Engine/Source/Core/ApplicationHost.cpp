#include "Core/ApplicationHost.h"
#include "Core/Concurrency/AsyncIO.h"
#include "Core/Concurrency/JobSystem.h"
#include "Core/CrashDiagnostics.h"
#include "Platform/PlatformDetection.h"

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Life
{
    namespace
    {
        struct SharedEngineSystemsState final
        {
            std::mutex Mutex;
            std::size_t ActiveHostCount = 0;
        };

        SharedEngineSystemsState& GetSharedEngineSystemsState()
        {
            static SharedEngineSystemsState state;
            return state;
        }

        void AcquireSharedEngineSystems(const ConcurrencySpecification& specification)
        {
            SharedEngineSystemsState& state = GetSharedEngineSystemsState();
            std::scoped_lock lock(state.Mutex);
            if (state.ActiveHostCount == 0)
            {
                GetJobSystem().Initialize(specification.JobWorkerCount);
                Async::GetAsyncIO().Initialize(specification.AsyncWorkerCount);
            }

            ++state.ActiveHostCount;
        }

        void ReleaseSharedEngineSystems()
        {
            SharedEngineSystemsState& state = GetSharedEngineSystemsState();
            std::scoped_lock lock(state.Mutex);
            if (state.ActiveHostCount == 0)
                return;

            --state.ActiveHostCount;
            if (state.ActiveHostCount == 0)
            {
                Async::GetAsyncIO().Shutdown();
                GetJobSystem().Shutdown();
            }
        }

        std::vector<std::string> ToCommandLineVector(const ApplicationCommandLineArgs& args)
        {
            std::vector<std::string> commandLine;
            commandLine.reserve(static_cast<std::size_t>(std::max(args.Count, 0)));

            for (int index = 0; index < args.Count; ++index)
            {
                if (args[index] != nullptr)
                    commandLine.emplace_back(args[index]);
            }

            return commandLine;
        }

        bool IsDefaultCrashReportingSpecification(const CrashReportingSpecification& specification)
        {
            const CrashReportingSpecification defaultSpecification{};
            return specification.Enabled == defaultSpecification.Enabled
                && specification.InstallHandlers == defaultSpecification.InstallHandlers
                && specification.CaptureSignals == defaultSpecification.CaptureSignals
                && specification.CaptureTerminate == defaultSpecification.CaptureTerminate
                && specification.CaptureUnhandledExceptions == defaultSpecification.CaptureUnhandledExceptions
                && specification.WriteReport == defaultSpecification.WriteReport
                && specification.WriteMiniDump == defaultSpecification.WriteMiniDump
                && specification.ReportDirectory == defaultSpecification.ReportDirectory
                && specification.MaxStackFrames == defaultSpecification.MaxStackFrames;
        }

        void RegisterBuiltInServices(
            ServiceRegistry& services,
            ApplicationHost& host,
            Application& application,
            ApplicationContext& context,
            ApplicationEventRouter& eventRouter,
            JobSystem& jobSystem,
            Async::AsyncIO& asyncIO,
            ApplicationRuntime& runtime,
            Window& window)
        {
            services.Register<ApplicationHost>(host);
            services.Register<Application>(application);
            services.Register<ApplicationContext>(context);
            services.Register<ApplicationEventRouter>(eventRouter);
            services.Register<JobSystem>(jobSystem);
            services.Register<Async::AsyncIO>(asyncIO);
            services.Register<ApplicationRuntime>(runtime);
            services.Register<Window>(window);
        }
    }

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
        CrashDiagnostics::Install();
        CrashDiagnostics::SetApplicationInfo(specification.Name, ToCommandLineVector(specification.CommandLineArgs));
        if (!IsDefaultCrashReportingSpecification(specification.CrashReporting))
            CrashDiagnostics::Configure(specification.CrashReporting);
        PlatformDetection::Initialize();
        LOG_CORE_INFO("Constructed application '{}'", specification.Name);
        m_Window = m_Runtime->CreatePlatformWindow(WindowSpecification
        {
            specification.Name,
            specification.Width,
            specification.Height,
            specification.VSync
        });

        AcquireSharedEngineSystems(specification.Concurrency);
        bool pushedGlobalServices = false;
        try
        {
            RegisterBuiltInServices(m_Services, *this, *m_Application, m_Context, m_EventRouter, GetJobSystem(), Async::GetAsyncIO(), *m_Runtime, *m_Window);
            PushGlobalServiceRegistry(m_Services);
            pushedGlobalServices = true;

            m_Context.Bind(
                *m_Window,
                *m_Runtime,
                m_Services,
                ApplicationContext::StateBinding{ m_Running, m_Initialized },
                [this]() { Initialize(); },
                [this](float timestep) { RunFrame(timestep); },
                [this]() { Shutdown(); },
                [this]() { Finalize(); });

            m_Application->BindHost(m_Context, m_EventRouter);
        }
        catch (...)
        {
            if (pushedGlobalServices)
                PopGlobalServiceRegistry(m_Services);
            ReleaseSharedEngineSystems();
            throw;
        }
    }

    ApplicationHost::~ApplicationHost()
    {
        Finalize();
        ReleaseSharedEngineSystems();
        PopGlobalServiceRegistry(m_Services);
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
