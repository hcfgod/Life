#include "Core/ApplicationHost.h"
#include "Assets/AssetHotReloadManager.h"
#include "Core/Concurrency/AsyncIO.h"
#include "Core/Concurrency/JobSystem.h"
#include "Core/CrashDiagnostics.h"
#include "Core/Error.h"
#include "Platform/PlatformDetection.h"

#include <algorithm>
#include <cstdio>
#include <exception>
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

        struct ActiveApplicationHostState final
        {
            std::mutex Mutex;
            ApplicationHost* ActiveHost = nullptr;
        };

        SharedEngineSystemsState& GetSharedEngineSystemsState()
        {
            static SharedEngineSystemsState state;
            return state;
        }

        ActiveApplicationHostState& GetActiveApplicationHostState()
        {
            static ActiveApplicationHostState state;
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

        void RegisterActiveApplicationHost(ApplicationHost& host)
        {
            ActiveApplicationHostState& state = GetActiveApplicationHostState();
            std::scoped_lock lock(state.Mutex);
            if (state.ActiveHost != nullptr)
            {
                throw Error(
                    ErrorCode::InvalidState,
                    "Life supports only one live ApplicationHost per process. Destroy the current host before creating another.",
                    std::source_location::current(),
                    ErrorSeverity::Critical);
            }

            state.ActiveHost = &host;
        }

        void UnregisterActiveApplicationHost(ApplicationHost& host) noexcept
        {
            ActiveApplicationHostState& state = GetActiveApplicationHostState();
            std::scoped_lock lock(state.Mutex);
            if (state.ActiveHost == &host)
                state.ActiveHost = nullptr;
        }

        void ReportApplicationHostTeardownException(const std::exception& exception) noexcept
        {
            try
            {
                CrashDiagnostics::ReportHandledException(exception, "ApplicationHost::~ApplicationHost");
            }
            catch (...)
            {
                std::fprintf(stderr, "Failed to report ApplicationHost teardown exception to crash diagnostics.\n");
            }

            try
            {
                LOG_CORE_ERROR("ApplicationHost teardown suppressed an exception: {}", exception.what());
            }
            catch (...)
            {
                std::fprintf(stderr, "ApplicationHost teardown suppressed an exception: %s\n", exception.what());
            }
        }

        void ReportApplicationHostTeardownException() noexcept
        {
            std::fprintf(stderr, "ApplicationHost teardown suppressed a non-standard exception.\n");
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

        void RegisterBuiltInServices(
            ServiceRegistry& services,
            ApplicationHost& host,
            Application& application,
            ApplicationContext& context,
            ApplicationEventRouter& eventRouter,
            LayerStack& layerStack,
            InputSystem& inputSystem,
            JobSystem& jobSystem,
            Async::AsyncIO& asyncIO,
            ApplicationRuntime& runtime,
            Window& window)
        {
            services.Register<ApplicationHost>(host);
            services.Register<Application>(application);
            services.Register<ApplicationContext>(context);
            services.Register<ApplicationEventRouter>(eventRouter);
            services.Register<LayerStack>(layerStack);
            services.Register<InputSystem>(inputSystem);
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
          m_Runtime(std::move(runtime)),
          m_Finalizing(false),
          m_SharedSystemsAcquired(false),
          m_GlobalServicesRegistered(false),
          m_RegisteredAsActiveHost(false)
    {
        if (m_Application == nullptr)
            throw std::logic_error("ApplicationHost requires a valid application instance.");

        if (m_Runtime == nullptr)
            throw std::logic_error("ApplicationHost requires a valid application runtime.");

        m_LayerStack.BindApplication(*m_Application);
        const ApplicationSpecification& specification = m_Application->GetSpecification();
        RegisterActiveApplicationHost(*this);
        m_RegisteredAsActiveHost = true;
        try
        {
            Log::Configure(specification.Logging);
            CrashDiagnostics::Install();
            CrashDiagnostics::SetApplicationInfo(specification.Name, ToCommandLineVector(specification.CommandLineArgs));
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

            try
            {
                GraphicsDeviceSpecification graphicsSpec;
                graphicsSpec.EnableValidation = true;
                graphicsSpec.VSync = specification.VSync;
                m_GraphicsDevice = CreateGraphicsDevice(graphicsSpec, *m_Window);
            }
            catch (const std::exception& e)
            {
                LOG_CORE_WARN("Graphics device creation failed ({}). Continuing without GPU rendering.", e.what());
            }

            AcquireSharedEngineSystems(specification.Concurrency);
            m_SharedSystemsAcquired = true;
            RegisterBuiltInServices(m_Services, *this, *m_Application, m_Context, m_EventRouter, m_LayerStack, m_InputSystem, GetJobSystem(), Async::GetAsyncIO(), *m_Runtime, *m_Window);
            m_Services.Register<Assets::AssetDatabase>(m_AssetDatabase);
            m_Services.Register<Assets::AssetBundle>(m_AssetBundle);
            m_Services.Register<Assets::AssetManager>(m_AssetManager);
            m_CameraManager = CreateScope<CameraManager>();
            m_Services.Register<CameraManager>(*m_CameraManager);
            m_ImGuiSystem = CreateScope<ImGuiSystem>(*m_Window, m_GraphicsDevice.get());
            m_Services.Register<ImGuiSystem>(*m_ImGuiSystem);
            if (m_GraphicsDevice)
            {
                m_Services.Register<GraphicsDevice>(*m_GraphicsDevice);
                try
                {
                    m_Renderer = CreateScope<Renderer>(*m_GraphicsDevice);
                    m_Services.Register<Renderer>(*m_Renderer);
                    m_Renderer2D = CreateScope<Renderer2D>(*m_Renderer);
                    m_Services.Register<Renderer2D>(*m_Renderer2D);
                }
                catch (const std::exception& e)
                {
                    LOG_CORE_WARN("Renderer service creation failed ({}). Continuing without renderer services.", e.what());
                }
            }
            m_InputSystem.SyncConnectedGamepads();
            SetGlobalServiceRegistry(&m_Services);
            m_GlobalServicesRegistered = true;
            Assets::AssetHotReloadManager::GetInstance().Enable(true);

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
            if (m_GlobalServicesRegistered)
            {
                Assets::AssetHotReloadManager::GetInstance().Enable(false);
                SetGlobalServiceRegistry(nullptr);
                m_GlobalServicesRegistered = false;
            }

            if (m_SharedSystemsAcquired)
            {
                ReleaseSharedEngineSystems();
                m_SharedSystemsAcquired = false;
            }

            if (m_RegisteredAsActiveHost)
            {
                UnregisterActiveApplicationHost(*this);
                m_RegisteredAsActiveHost = false;
            }

            throw;
        }
    }

    ApplicationHost::~ApplicationHost() noexcept
    {
        try
        {
            Finalize();
        }
        catch (const std::exception& exception)
        {
            ReportApplicationHostTeardownException(exception);
        }
        catch (...)
        {
            ReportApplicationHostTeardownException();
        }

        if (m_GlobalServicesRegistered)
        {
            Assets::AssetHotReloadManager::GetInstance().Enable(false);
            SetGlobalServiceRegistry(nullptr);
            m_GlobalServicesRegistered = false;
        }

        if (m_SharedSystemsAcquired)
        {
            ReleaseSharedEngineSystems();
            m_SharedSystemsAcquired = false;
        }

        if (m_RegisteredAsActiveHost)
        {
            UnregisterActiveApplicationHost(*this);
            m_RegisteredAsActiveHost = false;
        }

        m_ImGuiSystem.reset();
        m_Renderer2D.reset();
        m_CameraManager.reset();
        m_Renderer.reset();
        m_GraphicsDevice.reset();
        m_Window.reset();
        m_Runtime.reset();
        m_Application.reset();
    }

    void ApplicationHost::Initialize()
    {
        if (m_Initialized)
            return;

        m_Running = true;
        try
        {
            if (m_ImGuiSystem)
                m_ImGuiSystem->Initialize();

            m_Application->OnHostInitialize();
            m_Initialized = true;
        }
        catch (...)
        {
            m_Running = false;
            m_Initialized = false;
            if (m_ImGuiSystem)
                m_ImGuiSystem->Shutdown();
            try
            {
                m_LayerStack.Clear();
            }
            catch (const std::exception& exception)
            {
                LOG_CORE_ERROR("Failed to clear layers during initialization cleanup: {}", exception.what());
            }
            catch (...)
            {
                LOG_CORE_ERROR("Failed to clear layers during initialization cleanup due to an unknown exception.");
            }
            throw;
        }
    }

    void ApplicationHost::RunFrame(float timestep)
    {
        if (!m_Running || !m_Initialized)
            return;

        struct InputFrameFinalizer final
        {
            explicit InputFrameFinalizer(InputSystem& inputSystem)
                : Input(inputSystem)
            {
            }

            ~InputFrameFinalizer()
            {
                Input.EndFrame();
            }

            InputSystem& Input;
        } inputFrameFinalizer(m_InputSystem);

        m_InputSystem.SetKeyboardInputBlocked(m_ImGuiSystem && m_ImGuiSystem->WantsKeyboardCapture());
        m_InputSystem.SetMouseInputBlocked(m_ImGuiSystem && m_ImGuiSystem->WantsMouseCapture());
        m_InputSystem.UpdateActions();

        bool frameStarted = false;
        if (m_GraphicsDevice)
        {
            try { frameStarted = m_GraphicsDevice->BeginFrame(); }
            catch (const std::exception& e) { LOG_CORE_ERROR("BeginFrame failed: {}", e.what()); }
        }

        if (frameStarted && m_ImGuiSystem)
            m_ImGuiSystem->BeginFrame();

        m_Application->OnHostRunFrame(timestep);
        if (m_Running)
            m_LayerStack.OnUpdate(timestep);

        if (frameStarted && m_Running)
            m_LayerStack.OnRender();

        m_InputSystem.SetKeyboardInputBlocked(m_ImGuiSystem && m_ImGuiSystem->WantsKeyboardCapture());
        m_InputSystem.SetMouseInputBlocked(m_ImGuiSystem && m_ImGuiSystem->WantsMouseCapture());

        if (frameStarted && m_Running && m_ImGuiSystem)
            m_ImGuiSystem->Render();

        if (frameStarted && m_GraphicsDevice)
        {
            try { m_GraphicsDevice->Present(); }
            catch (const std::exception& e) { LOG_CORE_ERROR("Present failed: {}", e.what()); }
        }
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
        if (!m_Initialized || m_Finalizing)
            return;

        m_Finalizing = true;
        m_Running = false;
        std::exception_ptr finalizationFailure;
        try
        {
            m_Application->OnHostFinalize();
        }
        catch (...)
        {
            finalizationFailure = std::current_exception();
        }

        try
        {
            m_LayerStack.Clear();
        }
        catch (...)
        {
            if (finalizationFailure == nullptr)
                finalizationFailure = std::current_exception();
        }

        try
        {
            if (m_ImGuiSystem)
                m_ImGuiSystem->Shutdown();
        }
        catch (...)
        {
            if (finalizationFailure == nullptr)
                finalizationFailure = std::current_exception();
        }

        m_Initialized = false;
        m_Finalizing = false;

        if (finalizationFailure != nullptr)
            std::rethrow_exception(finalizationFailure);
    }
}
