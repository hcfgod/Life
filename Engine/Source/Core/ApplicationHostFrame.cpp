#include "Core/Detail/ApplicationHostFrameController.h"

#include "Core/ApplicationHost.h"

#include "Assets/AssetHotReloadManager.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/ImGuiSystem.h"

#include <exception>

namespace Life
{
    namespace
    {
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
            UpdateInputCaptureState();
            RunImGuiRenderPhase(frameStarted);
            RunPresentPhase(frameStarted);
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
            if (frameStarted && m_Host.m_ImGuiSystem)
                m_Host.m_ImGuiSystem->BeginFrame();
        }

        void ApplicationHostFrameController::RunApplicationUpdatePhase(float timestep)
        {
            m_Host.RunApplicationUpdateHook(timestep);
        }

        void ApplicationHostFrameController::RunAssetHotReloadPhase()
        {
            if (m_Host.m_Running)
                Assets::AssetHotReloadManager::GetInstance().Pump();
        }

        void ApplicationHostFrameController::RunLayerUpdatePhase(float timestep)
        {
            if (m_Host.m_Running)
                m_Host.m_LayerStack.OnUpdate(timestep);
        }

        void ApplicationHostFrameController::RunLayerRenderPhase(bool frameStarted)
        {
            if (frameStarted && m_Host.m_Running)
                m_Host.m_LayerStack.OnRender();
        }

        void ApplicationHostFrameController::RunImGuiRenderPhase(bool frameStarted)
        {
            if (frameStarted && m_Host.m_Running && m_Host.m_ImGuiSystem)
                m_Host.m_ImGuiSystem->Render();
        }

        void ApplicationHostFrameController::RunPresentPhase(bool frameStarted) noexcept
        {
            if (!frameStarted || !m_Host.m_GraphicsDevice)
                return;

            try
            {
                m_Host.m_GraphicsDevice->Present();
            }
            catch (const std::exception& e)
            {
                LOG_CORE_ERROR("Present failed: {}", e.what());
            }
        }
    }

    void ApplicationHost::RunFrame(float timestep)
    {
        Detail::ApplicationHostFrameController(*this).RunFrame(timestep);
    }
}
