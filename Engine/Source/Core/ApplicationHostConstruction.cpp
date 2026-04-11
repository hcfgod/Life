#include "Core/Detail/ApplicationHostConstruction.h"

#include "Core/ApplicationHost.h"

#include "Assets/AssetHotReloadManager.h"
#include "Assets/ProjectService.h"
#include "Core/ApplicationRuntime.h"
#include "Core/Concurrency/AsyncIO.h"
#include "Core/Concurrency/JobSystem.h"
#include "Core/CrashDiagnostics.h"
#include "Core/Error.h"
#include "Core/Window.h"
#include "Graphics/CameraManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ImGuiSystem.h"
#include "Graphics/Renderer.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/SceneRenderer2D.h"
#include "Platform/PlatformDetection.h"

#include <algorithm>
#include <exception>
#include <mutex>
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

    namespace Detail
    {
        ApplicationHostConstruction::ApplicationHostConstruction(ApplicationHost& host)
            : m_Host(host)
        {
        }

        void ApplicationHostConstruction::Run(const ApplicationSpecification& specification)
        {
            RegisterActiveApplicationHost(m_Host);
            m_Host.m_RegisteredAsActiveHost = true;
            ConfigureConstructionEnvironment(specification);
            CreatePlatformWindowAndGraphicsDevice(specification);
            AcquireAndRegisterCoreServices(specification);
            CreateAndRegisterHostServices();
            EnableGlobalServicesAndBindContext();
        }

        void ApplicationHostConstruction::CleanupFailure() noexcept
        {
            const auto closeProjectResult = m_Host.m_ProjectService.CloseProject();
            (void)closeProjectResult;
            m_Host.m_ProjectService.UnbindAssetSystems();
            DisableGlobalServices();
            ReleaseSharedSystems();
            UnregisterActiveHost();
        }

        void ApplicationHostConstruction::Teardown() noexcept
        {
            const auto closeProjectResult = m_Host.m_ProjectService.CloseProject();
            (void)closeProjectResult;
            m_Host.m_ProjectService.UnbindAssetSystems();
            DisableGlobalServices();
            ReleaseSharedSystems();
            UnregisterActiveHost();
            ResetOwnedServices();
        }

        void ApplicationHostConstruction::ConfigureConstructionEnvironment(const ApplicationSpecification& specification)
        {
            Log::Configure(specification.Logging);
            CrashDiagnostics::Install();
            CrashDiagnostics::SetApplicationInfo(specification.Name, ToCommandLineVector(specification.CommandLineArgs));
            CrashDiagnostics::Configure(specification.CrashReporting);
            PlatformDetection::Initialize();
            LOG_CORE_INFO("Constructed application '{}'", specification.Name);
        }

        void ApplicationHostConstruction::CreatePlatformWindowAndGraphicsDevice(const ApplicationSpecification& specification)
        {
            m_Host.m_Window = m_Host.m_Runtime->CreatePlatformWindow(WindowSpecification
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
                m_Host.m_GraphicsDevice = CreateGraphicsDevice(graphicsSpec, *m_Host.m_Window);
            }
            catch (const std::exception& e)
            {
                LOG_CORE_WARN("Graphics device creation failed ({}). Continuing without GPU rendering.", e.what());
            }
        }

        void ApplicationHostConstruction::AcquireAndRegisterCoreServices(const ApplicationSpecification& specification)
        {
            AcquireSharedEngineSystems(specification.Concurrency);
            m_Host.m_SharedSystemsAcquired = true;

            RegisterBuiltInServices(
                m_Host.m_Services,
                m_Host,
                *m_Host.m_Application,
                m_Host.m_Context,
                m_Host.m_EventRouter,
                m_Host.m_LayerStack,
                m_Host.m_InputSystem,
                GetJobSystem(),
                Async::GetAsyncIO(),
                *m_Host.m_Runtime,
                *m_Host.m_Window);

            m_Host.m_Services.Register<Assets::AssetDatabase>(m_Host.m_AssetDatabase);
            m_Host.m_Services.Register<Assets::AssetBundle>(m_Host.m_AssetBundle);
            m_Host.m_AssetManager.BindDatabase(m_Host.m_AssetDatabase);
            m_Host.m_Services.Register<Assets::AssetManager>(m_Host.m_AssetManager);
            m_Host.m_ProjectService.BindAssetSystems(m_Host.m_AssetDatabase, m_Host.m_AssetManager);
            m_Host.m_Services.Register<Assets::ProjectService>(m_Host.m_ProjectService);

            if (!specification.ProjectDescriptorPath.empty())
            {
                const auto openProjectResult = m_Host.m_ProjectService.OpenProject(specification.ProjectDescriptorPath);
                if (openProjectResult.IsFailure())
                {
                    throw Error(
                        openProjectResult.GetError().GetCode(),
                        "Failed to open project descriptor '" + specification.ProjectDescriptorPath.string() + "': " +
                            openProjectResult.GetError().GetErrorMessage(),
                        std::source_location::current(),
                        openProjectResult.GetError().GetSeverity());
                }
            }
        }

        void ApplicationHostConstruction::CreateAndRegisterHostServices()
        {
            m_Host.m_CameraManager = CreateScope<CameraManager>();
            m_Host.m_Services.Register<CameraManager>(*m_Host.m_CameraManager);

            m_Host.m_ImGuiSystem = CreateScope<ImGuiSystem>(*m_Host.m_Window, m_Host.m_GraphicsDevice.get());
            m_Host.m_Services.Register<ImGuiSystem>(*m_Host.m_ImGuiSystem);

            if (!m_Host.m_GraphicsDevice)
                return;

            m_Host.m_Services.Register<GraphicsDevice>(*m_Host.m_GraphicsDevice);
            try
            {
                m_Host.m_Renderer = CreateScope<Renderer>(*m_Host.m_GraphicsDevice);
                m_Host.m_Services.Register<Renderer>(*m_Host.m_Renderer);
                m_Host.m_Renderer2D = CreateScope<Renderer2D>(*m_Host.m_Renderer);
                m_Host.m_Services.Register<Renderer2D>(*m_Host.m_Renderer2D);
                m_Host.m_SceneRenderer2D = CreateScope<SceneRenderer2D>(*m_Host.m_Renderer2D);
                m_Host.m_Services.Register<SceneRenderer2D>(*m_Host.m_SceneRenderer2D);
            }
            catch (const std::exception& e)
            {
                LOG_CORE_WARN("Renderer service creation failed ({}). Continuing without renderer services.", e.what());
            }
        }

        void ApplicationHostConstruction::EnableGlobalServicesAndBindContext()
        {
            m_Host.m_InputSystem.SyncConnectedGamepads();
            SetGlobalServiceRegistry(&m_Host.m_Services);
            m_Host.m_GlobalServicesRegistered = true;
            Assets::AssetHotReloadManager::GetInstance().Enable(true);
            ApplicationHost* host = &m_Host;

            m_Host.m_Context.Bind(
                *m_Host.m_Window,
                *m_Host.m_Runtime,
                m_Host.m_Services,
                ApplicationContext::StateBinding{ m_Host.m_Running, m_Host.m_Initialized },
                [host]() { host->Initialize(); },
                [host](float timestep) { host->RunFrame(timestep); },
                [host]() { host->Shutdown(); },
                [host]() { host->Finalize(); });

            m_Host.BindApplicationHostState();
        }

        void ApplicationHostConstruction::DisableGlobalServices() noexcept
        {
            if (!m_Host.m_GlobalServicesRegistered)
                return;

            Assets::AssetHotReloadManager::GetInstance().Enable(false);
            SetGlobalServiceRegistry(nullptr);
            m_Host.m_GlobalServicesRegistered = false;
        }

        void ApplicationHostConstruction::ReleaseSharedSystems() noexcept
        {
            if (!m_Host.m_SharedSystemsAcquired)
                return;

            ReleaseSharedEngineSystems();
            m_Host.m_SharedSystemsAcquired = false;
        }

        void ApplicationHostConstruction::UnregisterActiveHost() noexcept
        {
            if (!m_Host.m_RegisteredAsActiveHost)
                return;

            UnregisterActiveApplicationHost(m_Host);
            m_Host.m_RegisteredAsActiveHost = false;
        }

        void ApplicationHostConstruction::ResetOwnedServices() noexcept
        {
            m_Host.m_ImGuiSystem.reset();
            m_Host.m_SceneRenderer2D.reset();
            m_Host.m_Renderer2D.reset();
            m_Host.m_CameraManager.reset();
            m_Host.m_Renderer.reset();
            m_Host.m_GraphicsDevice.reset();
            m_Host.m_Window.reset();
            m_Host.m_Runtime.reset();
            m_Host.m_Application.reset();
        }
    }
}
