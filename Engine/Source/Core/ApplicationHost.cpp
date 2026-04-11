#include "Core/ApplicationHost.h"

#include "Core/ApplicationRuntime.h"
#include "Core/CrashDiagnostics.h"
#include "Core/Detail/ApplicationHostConstruction.h"
#include "Core/Window.h"
#include "Graphics/CameraManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ImGuiSystem.h"
#include "Graphics/Renderer.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/SceneRenderer2D.h"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

namespace Life
{
    namespace
    {
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
        Detail::ApplicationHostConstruction construction(*this);
        try
        {
            construction.Run(specification);
        }
        catch (...)
        {
            construction.CleanupFailure();
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

        Detail::ApplicationHostConstruction(*this).Teardown();
    }

    void ApplicationHost::HandleEvent(Event& event)
    {
        m_EventRouter.Route(*m_Application, event);
    }

    void ApplicationHost::Shutdown()
    {
        m_Running = false;
    }

    void ApplicationHost::BindApplicationHostState()
    {
        m_Application->BindHost(m_Context, m_EventRouter);
    }

    void ApplicationHost::RunApplicationInitializeHook()
    {
        m_Application->OnHostInitialize();
    }

    void ApplicationHost::RunApplicationUpdateHook(float timestep)
    {
        m_Application->OnHostRunFrame(timestep);
    }

    void ApplicationHost::RunApplicationFinalizeHook()
    {
        m_Application->OnHostFinalize();
    }
}
