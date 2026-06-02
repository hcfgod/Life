#include "Core/Detail/ApplicationHostLifecycleController.h"

#include "Core/ApplicationHost.h"

#include "Graphics/ImGuiSystem.h"

#include <cstdio>
#include <exception>

namespace Life
{
    namespace
    {
        void ReportSuppressedLoggingFailure(const char* message) noexcept
        {
            (void)std::fputs(message, stderr);
            (void)std::fputc('\n', stderr);
            std::fflush(stderr);
        }
    }

    namespace Detail
    {
        ApplicationHostLifecycleController::ApplicationHostLifecycleController(ApplicationHost& host)
            : m_Host(host)
        {
        }

        void ApplicationHostLifecycleController::Initialize()
        {
            if (m_Host.m_Initialized)
                return;

            m_Host.m_Running = true;
            try
            {
                if (m_Host.m_ImGuiSystem)
                    m_Host.m_ImGuiSystem->Initialize();

                m_Host.RunApplicationInitializeHook();
                m_Host.m_Initialized = true;
            }
            catch (...)
            {
                m_Host.m_Running = false;
                m_Host.m_Initialized = false;
                if (m_Host.m_ImGuiSystem)
                    m_Host.m_ImGuiSystem->Shutdown();
                ClearLayersAfterInitializationFailure();
                throw;
            }
        }

        void ApplicationHostLifecycleController::Finalize()
        {
            if (!m_Host.m_Initialized || m_Host.m_Finalizing)
                return;

            m_Host.m_Finalizing = true;
            m_Host.m_Running = false;
            std::exception_ptr finalizationFailure;
            try
            {
                m_Host.RunApplicationFinalizeHook();
            }
            catch (...)
            {
                finalizationFailure = std::current_exception();
            }

            try
            {
                m_Host.m_LayerStack.Clear();
            }
            catch (...)
            {
                if (finalizationFailure == nullptr)
                    finalizationFailure = std::current_exception();
            }

            try
            {
                if (m_Host.m_ImGuiSystem)
                    m_Host.m_ImGuiSystem->Shutdown();
            }
            catch (...)
            {
                if (finalizationFailure == nullptr)
                    finalizationFailure = std::current_exception();
            }

            m_Host.m_Initialized = false;
            m_Host.m_Finalizing = false;

            if (finalizationFailure != nullptr)
                std::rethrow_exception(finalizationFailure);
        }

        void ApplicationHostLifecycleController::ClearLayersAfterInitializationFailure() noexcept
        {
            try
            {
                m_Host.m_LayerStack.Clear();
            }
            catch (const std::exception& exception)
            {
                try
                {
                    LOG_CORE_ERROR("Failed to clear layers during initialization cleanup: {}", exception.what());
                }
                catch (...)
                {
                    ReportSuppressedLoggingFailure("Failed to report layer cleanup exception.");
                }
            }
            catch (...)
            {
                try
                {
                    LOG_CORE_ERROR("Failed to clear layers during initialization cleanup due to an unknown exception.");
                }
                catch (...)
                {
                    ReportSuppressedLoggingFailure("Failed to report unknown layer cleanup exception.");
                }
            }
        }
    }

    void ApplicationHost::Initialize()
    {
        Detail::ApplicationHostLifecycleController(*this).Initialize();
    }

    void ApplicationHost::Finalize()
    {
        Detail::ApplicationHostLifecycleController(*this).Finalize();
    }
}
