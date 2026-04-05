#include "Editor/EditorShellOverlay.h"

#include <nvrhi/nvrhi.h>

#if __has_include(<imgui.h>)
#include <imgui.h>
#include <imgui_internal.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>

namespace EditorApp
{
    EditorShellOverlay::EditorShellOverlay()
        : Life::Layer("EditorShellOverlay")
    {
    }

    void EditorShellOverlay::OnAttach()
    {
        m_LayoutInitialized = false;
        EnsureEditorCamera();
        LOG_INFO("Editor shell overlay attached.");
    }

    void EditorShellOverlay::OnDetach()
    {
        ReleaseSceneRenderTarget();

        Life::Application& application = GetApplication();
        if (m_OwnsCamera)
        {
            if (Life::CameraManager* cameraManager = application.TryGetService<Life::CameraManager>())
                cameraManager->DestroyCamera(m_EditorCameraName);
        }

        m_OwnsCamera = false;
        LOG_INFO("Editor shell overlay detached.");
    }

    void EditorShellOverlay::OnUpdate(float timestep)
    {
        m_ElapsedTime += timestep;

        if (Life::InputSystem* inputSystem = GetApplication().TryGetService<Life::InputSystem>())
        {
            if (inputSystem->WasActionStartedThisFrame("Editor", "Quit"))
                GetApplication().RequestShutdown();
        }
    }

    void EditorShellOverlay::OnRender()
    {
#if __has_include(<imgui.h>)
        Life::Application& application = GetApplication();
        Life::ImGuiSystem* imguiSystem = application.TryGetService<Life::ImGuiSystem>();
        if (imguiSystem == nullptr || !imguiSystem->IsInitialized() || !imguiSystem->IsAvailable())
            return;

        EnsureEditorCamera();

        ImGuiWindowFlags dockspaceWindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(1.0f);

        dockspaceWindowFlags |= ImGuiWindowFlags_NoTitleBar;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoCollapse;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoResize;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoMove;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        dockspaceWindowFlags |= ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorRoot", nullptr, dockspaceWindowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        if (!m_LayoutInitialized)
        {
            m_LayoutInitialized = true;
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

            ImGuiID rootDockId = dockspaceId;
            ImGuiID leftDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Left, 0.20f, nullptr, &rootDockId);
            ImGuiID rightDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Right, 0.24f, nullptr, &rootDockId);
            ImGuiID bottomDockId = ImGui::DockBuilderSplitNode(rootDockId, ImGuiDir_Down, 0.28f, nullptr, &rootDockId);

            ImGui::DockBuilderDockWindow("Hierarchy", leftDockId);
            ImGui::DockBuilderDockWindow("Inspector", rightDockId);
            ImGui::DockBuilderDockWindow("Console", bottomDockId);
            ImGui::DockBuilderDockWindow("Stats", rightDockId);
            ImGui::DockBuilderDockWindow("Scene", rootDockId);
            ImGui::DockBuilderFinish(dockspaceId);
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchyPanel);
                ImGui::MenuItem("Inspector", nullptr, &m_ShowInspectorPanel);
                ImGui::MenuItem("Console", nullptr, &m_ShowConsolePanel);
                ImGui::MenuItem("Stats", nullptr, &m_ShowStatsPanel);
                ImGui::MenuItem("Scene", nullptr, &m_ShowScenePanel);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Life Editor");
            ImGui::EndMenuBar();
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();

        if (m_ShowHierarchyPanel)
        {
            if (ImGui::Begin("Hierarchy", &m_ShowHierarchyPanel))
            {
                const Life::LayerStack& layerStack = application.GetLayerStack();
                const std::size_t regularLayerCount = layerStack.GetRegularLayerCount();
                std::size_t index = 0;
                for (const Life::LayerRef& layer : layerStack)
                {
                    if (!layer)
                    {
                        ++index;
                        continue;
                    }

                    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (layer->IsEnabled())
                        nodeFlags |= ImGuiTreeNodeFlags_Bullet;

                    const bool isOverlay = index >= regularLayerCount;
                    std::string label = layer->GetDebugName();
                    label += isOverlay ? " (Overlay)" : " (Layer)";
                    ImGui::TreeNodeEx(label.c_str(), nodeFlags);
                    ++index;
                }
            }
            ImGui::End();
        }

        if (m_ShowInspectorPanel)
        {
            if (ImGui::Begin("Inspector", &m_ShowInspectorPanel))
            {
                if (Life::CameraManager* cameraManager = application.TryGetService<Life::CameraManager>())
                {
                    if (Life::Camera* editorCamera = cameraManager->GetCamera(m_EditorCameraName))
                    {
                        const glm::vec3& position = editorCamera->GetPosition();
                        ImGui::Text("Camera: %s", editorCamera->GetName().c_str());
                        ImGui::Text("Projection: %s", editorCamera->GetProjectionType() == Life::ProjectionType::Perspective ? "Perspective" : "Orthographic");
                        ImGui::Text("Position: %.2f %.2f %.2f", position.x, position.y, position.z);
                        ImGui::Text("Aspect Ratio: %.3f", editorCamera->GetAspectRatio());
                    }
                }
            }
            ImGui::End();
        }

        if (m_ShowConsolePanel)
        {
            if (ImGui::Begin("Console", &m_ShowConsolePanel))
            {
                ImGui::TextUnformatted("Editor viewport rendering is active.");
                ImGui::Separator();
                ImGui::TextUnformatted("ImGui is now hosted by the dedicated Editor app.");
            }
            ImGui::End();
        }

        if (m_ShowStatsPanel)
        {
            if (ImGui::Begin("Stats", &m_ShowStatsPanel))
            {
                ImGui::Text("Graphics Backend: %s", imguiSystem->GetBackend() == Life::GraphicsBackend::Vulkan ? "Vulkan" : imguiSystem->GetBackend() == Life::GraphicsBackend::D3D12 ? "D3D12" : "None");
                ImGui::Text("Viewport Size: %u x %u", m_ViewportWidth, m_ViewportHeight);
                ImGui::Text("ImGui Keyboard Capture: %s", imguiSystem->WantsKeyboardCapture() ? "true" : "false");
                ImGui::Text("ImGui Mouse Capture: %s", imguiSystem->WantsMouseCapture() ? "true" : "false");
                if (Life::Renderer2D* renderer2D = application.TryGetService<Life::Renderer2D>())
                {
                    const Life::Renderer2D::Statistics& stats = renderer2D->GetStats();
                    ImGui::Separator();
                    ImGui::Text("Renderer2D Draw Calls: %u", stats.DrawCalls);
                    ImGui::Text("Renderer2D Quads: %u", stats.QuadCount);
                }
            }
            ImGui::End();
        }

        if (m_ShowScenePanel)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            if (ImGui::Begin("Scene", &m_ShowScenePanel))
            {
                const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
                if (availableRegion.x >= 1.0f && availableRegion.y >= 1.0f)
                {
                    EnsureSceneRenderTarget(
                        static_cast<uint32_t>(availableRegion.x),
                        static_cast<uint32_t>(availableRegion.y));
                    RenderSceneViewport(*imguiSystem);
                    if (m_SceneTextureHandle != nullptr)
                        ImGui::Image(ImTextureRef(m_SceneTextureHandle), availableRegion, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
                }
                else
                {
                    ImGui::TextUnformatted("Scene viewport has no drawable area.");
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }
#endif
    }

    void EditorShellOverlay::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowResizeEvent>([this](Life::WindowResizeEvent&)
        {
            m_LayoutInitialized = false;
            return false;
        });
    }

    void EditorShellOverlay::EnsureEditorCamera()
    {
        Life::Application& application = GetApplication();
        Life::CameraManager* cameraManager = application.TryGetService<Life::CameraManager>();
        if (cameraManager == nullptr)
            return;

        Life::Camera* editorCamera = cameraManager->GetCamera(m_EditorCameraName);
        if (editorCamera == nullptr)
        {
            Life::CameraSpecification cameraSpecification;
            cameraSpecification.Name = m_EditorCameraName;
            cameraSpecification.Projection = Life::ProjectionType::Orthographic;
            cameraSpecification.AspectRatio = m_ViewportHeight > 0 ? static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight) : 16.0f / 9.0f;
            cameraSpecification.OrthoSize = 4.5f;
            cameraSpecification.OrthoNear = 0.1f;
            cameraSpecification.OrthoFar = 10.0f;
            cameraSpecification.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };

            editorCamera = cameraManager->CreateCamera(cameraSpecification);
            m_OwnsCamera = editorCamera != nullptr;
        }

        if (editorCamera != nullptr)
        {
            Life::OrthographicProjectionParameters orthographicParameters;
            orthographicParameters.Size = 4.5f;
            orthographicParameters.NearClip = 0.1f;
            orthographicParameters.FarClip = 10.0f;
            editorCamera->SetOrthographic(orthographicParameters);
            editorCamera->SetPosition({ 0.0f, 0.0f, 1.0f });
            editorCamera->LookAt({ 0.0f, 0.0f, 0.0f });
            cameraManager->SetPrimaryCamera(m_EditorCameraName);
        }
    }

    void EditorShellOverlay::EnsureSceneRenderTarget(uint32_t width, uint32_t height)
    {
        width = std::max(width, 1u);
        height = std::max(height, 1u);
        if (m_SceneColorTarget && m_ViewportWidth == width && m_ViewportHeight == height)
            return;

        ReleaseSceneRenderTarget();

        if (Life::GraphicsDevice* graphicsDevice = GetApplication().TryGetService<Life::GraphicsDevice>())
        {
            Life::TextureDescription textureDescription;
            textureDescription.DebugName = "EditorSceneColorTarget";
            textureDescription.Width = width;
            textureDescription.Height = height;
            textureDescription.Format = Life::TextureFormat::BGRA8_UNORM;
            textureDescription.IsRenderTarget = true;
            m_SceneColorTarget = Life::TextureResource::Create2D(*graphicsDevice, textureDescription);
            if (m_SceneColorTarget)
            {
                m_ViewportWidth = width;
                m_ViewportHeight = height;
                if (!m_LoggedSceneViewportReady)
                {
                    LOG_INFO("Editor scene viewport render target created at {}x{}.", width, height);
                    m_LoggedSceneViewportReady = true;
                }
                if (Life::CameraManager* cameraManager = GetApplication().TryGetService<Life::CameraManager>())
                {
                    if (Life::Camera* editorCamera = cameraManager->GetCamera(m_EditorCameraName))
                        editorCamera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
                }
            }
            else if (!m_LoggedSceneTargetFailure)
            {
                LOG_ERROR("Editor scene viewport render target creation failed at {}x{}.", width, height);
                m_LoggedSceneTargetFailure = true;
            }
        }
    }

    void EditorShellOverlay::ReleaseSceneRenderTarget() noexcept
    {
        if (m_SceneColorTarget)
        {
            if (Life::ImGuiSystem* imguiSystem = GetApplication().TryGetService<Life::ImGuiSystem>())
                imguiSystem->ReleaseTextureHandle(*m_SceneColorTarget);
        }

        m_SceneColorTarget.reset();
        m_SceneTextureHandle = nullptr;
        m_ViewportWidth = 0;
        m_ViewportHeight = 0;
        m_LoggedSceneViewportReady = false;
    }

    void EditorShellOverlay::RenderSceneViewport(Life::ImGuiSystem& imguiSystem)
    {
        Life::Application& application = GetApplication();
        Life::Renderer* renderer = application.TryGetService<Life::Renderer>();
        Life::Renderer2D* renderer2D = application.TryGetService<Life::Renderer2D>();
        Life::CameraManager* cameraManager = application.TryGetService<Life::CameraManager>();
        if (renderer == nullptr || renderer2D == nullptr || cameraManager == nullptr || !m_SceneColorTarget)
            return;

        Life::Camera* editorCamera = cameraManager->GetCamera(m_EditorCameraName);
        if (editorCamera == nullptr)
            return;

        if (nvrhi::ICommandList* commandList = renderer->GetGraphicsDevice().GetCurrentCommandList())
        {
            if (nvrhi::ITexture* nativeTexture = m_SceneColorTarget->GetNativeHandle())
            {
                commandList->setTextureState(nativeTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
                commandList->commitBarriers();
            }
        }

        renderer->SetRenderTarget(m_SceneColorTarget.get());
        renderer2D->BeginScene(*editorCamera);
        renderer2D->DrawQuad({ 0.0f, 0.0f, 0.0f }, { 3.5f, 3.5f }, { 0.20f, 0.55f, 0.95f, 1.0f });
        renderer2D->DrawRotatedQuad(
            { std::sin(m_ElapsedTime) * 1.75f, std::cos(m_ElapsedTime * 0.75f) * 1.25f, -0.5f },
            { 1.35f, 1.35f },
            m_ElapsedTime,
            { 0.95f, 0.45f, 0.25f, 0.90f });
        renderer2D->DrawQuad({ -2.0f, -1.4f, -1.0f }, { 1.25f, 1.25f }, { 0.25f, 0.90f, 0.45f, 0.85f });
        renderer2D->EndScene();

        if (nvrhi::ICommandList* commandList = renderer->GetGraphicsDevice().GetCurrentCommandList())
        {
            if (nvrhi::ITexture* nativeTexture = m_SceneColorTarget->GetNativeHandle())
            {
                commandList->setTextureState(nativeTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                commandList->commitBarriers();
            }
        }

        renderer->SetRenderTarget(nullptr);
        m_SceneTextureHandle = imguiSystem.GetTextureHandle(*m_SceneColorTarget);
        if (m_SceneTextureHandle == nullptr && !m_LoggedSceneHandleFailure)
        {
            LOG_ERROR("Editor scene viewport failed to acquire an ImGui texture handle for the render target.");
            m_LoggedSceneHandleFailure = true;
        }
    }
}
