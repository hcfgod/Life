#include "Core/Detail/ApplicationHostFrameController.h"

#include "Core/ApplicationHost.h"

#include "Assets/AssetContext.h"
#include "Graphics/GraphicsDevice.h"
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
        };
    }

    namespace Detail
    {
        ApplicationHostFrameController::ApplicationHostFrameController(ApplicationHost& host)
            : m_Host(host)
        {
        }

        void ApplicationHostFrameController::RunFrame(float timestep)
        {
            if (!m_Host.m_Running || !m_Host.m_Initialized)
                return;

            InputFrameFinalizer inputFrameFinalizer(m_Host.m_InputSystem);

            UpdateInputCaptureState();
            m_Host.m_InputSystem.UpdateActions();

            const bool frameStarted = TryBeginGraphicsFrame();
            BeginImGuiFramePhase(frameStarted);
            RunApplicationUpdatePhase(timestep);
            RunAssetHotReloadPhase();
            RunLayerUpdatePhase(timestep);
            RunLayerRenderPhase(frameStarted);
            if (HasLostGraphicsDevice())
            {
                StopAfterGraphicsDeviceLoss();
                return;
            }
            UpdateInputCaptureState();
            RunImGuiRenderPhase(frameStarted);
            if (HasLostGraphicsDevice())
            {
                StopAfterGraphicsDeviceLoss();
                return;
            }
            RunPresentPhase(frameStarted);
            if (HasLostGraphicsDevice())
                StopAfterGraphicsDeviceLoss();
        }

        void ApplicationHostFrameController::UpdateInputCaptureState() noexcept
        {
            m_Host.m_InputSystem.SetKeyboardInputBlocked(m_Host.m_ImGuiSystem && m_Host.m_ImGuiSystem->WantsKeyboardCapture());
            m_Host.m_InputSystem.SetMouseInputBlocked(m_Host.m_ImGuiSystem && m_Host.m_ImGuiSystem->WantsMouseCapture());
        }

        bool ApplicationHostFrameController::TryBeginGraphicsFrame() noexcept
        {
            if (!m_Host.m_GraphicsDevice)
                return false;

            try
            {
                return m_Host.m_GraphicsDevice->BeginFrame();
            }
            catch (const std::exception& e)
            {
                LOG_CORE_ERROR("BeginFrame failed: {}", e.what());
            }

            return false;
        }

        void ApplicationHostFrameController::BeginImGuiFramePhase(bool frameStarted)
        {
            if (frameStarted && !HasLostGraphicsDevice() && m_Host.m_ImGuiSystem)
                m_Host.m_ImGuiSystem->BeginFrame();
        }

        void ApplicationHostFrameController::RunApplicationUpdatePhase(float timestep)
        {
            m_Host.RunApplicationUpdateHook(timestep);
        }

        void ApplicationHostFrameController::RunAssetHotReloadPhase()
        {
            if (m_Host.m_Running)
                m_Host.m_AssetContext.GetHotReloadManager().Pump();
        }

        void ApplicationHostFrameController::RunLayerUpdatePhase(float timestep)
        {
            if (m_Host.m_Running)
                m_Host.m_LayerStack.OnUpdate(timestep);
        }

        void ApplicationHostFrameController::RunLayerRenderPhase(bool frameStarted)
        {
            if (frameStarted && m_Host.m_Running && !HasLostGraphicsDevice())
                m_Host.m_LayerStack.OnRender();
        }

        void ApplicationHostFrameController::RunImGuiRenderPhase(bool frameStarted)
        {
            if (!frameStarted || !m_Host.m_Running || HasLostGraphicsDevice() || !m_Host.m_ImGuiSystem)
                return;

            try
            {
                m_Host.m_ImGuiSystem->Render();
            }
            catch (const std::exception& exception)
            {
                LOG_CORE_ERROR("ImGui render failed: {}", exception.what());
                m_Host.m_Running = false;
            }
            catch (...)
            {
                LOG_CORE_ERROR("ImGui render failed due to an unknown exception.");
                m_Host.m_Running = false;
            }
        }

        void ApplicationHostFrameController::RunPresentPhase(bool frameStarted) noexcept
        {
            if (!frameStarted || !m_Host.m_Running || !m_Host.m_GraphicsDevice || HasLostGraphicsDevice())
                return;

            try
            {
                m_Host.m_GraphicsDevice->Present();
            }
            catch (const std::exception& e)
            {
                try
                {
                    LOG_CORE_ERROR("Present failed: {}", e.what());
                }
                catch (...)
                {
                    ReportSuppressedLoggingFailure("Failed to report present exception.");
                }
                m_Host.m_Running = false;
            }
        }

        bool ApplicationHostFrameController::HasLostGraphicsDevice() const noexcept
        {
            return m_Host.m_GraphicsDevice && m_Host.m_GraphicsDevice->IsDeviceLost();
        }

        void ApplicationHostFrameController::StopAfterGraphicsDeviceLoss() noexcept
        {
            if (!m_Host.m_Running)
                return;

            try
            {
                LOG_CORE_CRITICAL("Graphics device was lost; stopping the application to avoid submitting more GPU work.");
            }
            catch (...)
            {
                ReportSuppressedLoggingFailure("Failed to report graphics device loss.");
            }
            m_Host.m_Running = false;
        }
    }

    void ApplicationHost::RunFrame(float timestep)
    {
        Detail::ApplicationHostFrameController(*this).RunFrame(timestep);
    }
}
