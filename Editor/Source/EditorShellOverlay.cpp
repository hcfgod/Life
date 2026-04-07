#include "Editor/EditorShellOverlay.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#include <imgui_internal.h>
#endif

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
        Life::Application& application = GetApplication();
        Life::Assets::AssetManager& assetManager = application.GetService<Life::Assets::AssetManager>();
        m_CheckerTextureAsset = assetManager.GetOrLoad<Life::Assets::TextureAsset>(m_CheckerTextureKey);
        if (!m_CheckerTextureAsset)
            LOG_WARN("Editor failed to load textured quad asset '{}'. Falling back to error texture.", m_CheckerTextureKey);
        else
        {
            m_CheckerTextureAsset->SetFilterModes(Life::TextureFilterMode::Nearest, Life::TextureFilterMode::Nearest);
            m_CheckerTextureAsset->SetWrapModes(Life::TextureWrapMode::Repeat, Life::TextureWrapMode::Repeat);
        }

        if (application.HasService<Life::Renderer>() && application.HasService<Life::Renderer2D>())
        {
            m_SceneViewport = Life::CreateScope<Life::SceneViewport>(
                application.GetService<Life::Renderer>(),
                application.GetService<Life::Renderer2D>(),
                application.GetService<Life::ImGuiSystem>());
        }

        LOG_INFO("Editor shell overlay attached.");
    }

    void EditorShellOverlay::OnDetach()
    {
        m_SceneViewport.reset();

        if (m_OwnsCamera)
            GetApplication().GetService<Life::CameraManager>().DestroyCamera(m_EditorCameraName);

        m_OwnsCamera = false;
        m_CheckerTextureAsset.reset();
        LOG_INFO("Editor shell overlay detached.");
    }

    void EditorShellOverlay::OnUpdate(float timestep)
    {
        m_ElapsedTime += timestep;

        if (GetApplication().GetService<Life::InputSystem>().WasActionStartedThisFrame("Editor", "Quit"))
            GetApplication().RequestShutdown();
    }

    void EditorShellOverlay::OnRender()
    {
#if __has_include(<imgui.h>)
        Life::Application& application = GetApplication();
        Life::ImGuiSystem& imguiSystem = application.GetService<Life::ImGuiSystem>();
        if (!imguiSystem.IsInitialized() || !imguiSystem.IsAvailable())
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
                if (Life::Camera* editorCamera = TryGetEditorCamera())
                {
                    const glm::vec3& position = editorCamera->GetPosition();
                    ImGui::Text("Camera: %s", editorCamera->GetName().c_str());
                    ImGui::Text("Projection: %s", editorCamera->GetProjectionType() == Life::ProjectionType::Perspective ? "Perspective" : "Orthographic");
                    ImGui::Text("Position: %.2f %.2f %.2f", position.x, position.y, position.z);
                    ImGui::Text("Aspect Ratio: %.3f", editorCamera->GetAspectRatio());
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
                ImGui::Text("Graphics Backend: %s", imguiSystem.GetBackend() == Life::GraphicsBackend::Vulkan ? "Vulkan" : imguiSystem.GetBackend() == Life::GraphicsBackend::D3D12 ? "D3D12" : "None");
                ImGui::Text("Viewport Size: %u x %u", m_SceneViewport ? m_SceneViewport->GetWidth() : 0u, m_SceneViewport ? m_SceneViewport->GetHeight() : 0u);
                ImGui::Text("ImGui Keyboard Capture: %s", imguiSystem.WantsKeyboardCapture() ? "true" : "false");
                ImGui::Text("ImGui Mouse Capture: %s", imguiSystem.WantsMouseCapture() ? "true" : "false");
                if (m_SceneViewport)
                {
                    const Life::Renderer2D::Statistics& stats = application.GetService<Life::Renderer2D>().GetStats();
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
                    if (!RenderSceneViewport(
                            static_cast<uint32_t>(availableRegion.x),
                            static_cast<uint32_t>(availableRegion.y)))
                    {
                        ImGui::TextUnformatted("Scene viewport rendering is unavailable.");
                    }
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
        Life::CameraManager& cameraManager = GetApplication().GetService<Life::CameraManager>();

        Life::CameraSpecification cameraSpecification;
        cameraSpecification.Name = m_EditorCameraName;
        cameraSpecification.Projection = Life::ProjectionType::Orthographic;
        cameraSpecification.AspectRatio = (m_SceneViewport && m_SceneViewport->GetHeight() > 0)
            ? static_cast<float>(m_SceneViewport->GetWidth()) / static_cast<float>(m_SceneViewport->GetHeight())
            : 16.0f / 9.0f;
        cameraSpecification.OrthoSize = 4.5f;
        cameraSpecification.OrthoNear = 0.1f;
        cameraSpecification.OrthoFar = 10.0f;
        cameraSpecification.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };

        const bool alreadyExists = cameraManager.HasCamera(m_EditorCameraName);
        Life::Camera& editorCamera = cameraManager.EnsureCamera(cameraSpecification);
        if (!alreadyExists)
            m_OwnsCamera = true;

        Life::OrthographicProjectionParameters orthographicParameters;
        orthographicParameters.Size = 4.5f;
        orthographicParameters.NearClip = 0.1f;
        orthographicParameters.FarClip = 10.0f;
        editorCamera.SetOrthographic(orthographicParameters);
        editorCamera.SetPosition({ 0.0f, 0.0f, 1.0f });
        editorCamera.LookAt({ 0.0f, 0.0f, 0.0f });
        cameraManager.SetPrimaryCamera(m_EditorCameraName);
    }

    void EditorShellOverlay::DrawSceneViewportContent(Life::Renderer2D& renderer2D)
    {
        if (m_CheckerTextureAsset)
            renderer2D.DrawQuad({ 0.0f, 0.0f, 0.0f }, { 3.5f, 3.5f }, *m_CheckerTextureAsset, { 1.0f, 1.0f, 1.0f, 1.0f });
        else
            renderer2D.DrawQuad({ 0.0f, 0.0f, 0.0f }, { 3.5f, 3.5f }, { 1.0f, 0.0f, 1.0f, 1.0f });

        renderer2D.DrawRotatedQuad(
            { std::sin(m_ElapsedTime) * 1.75f, std::cos(m_ElapsedTime * 0.75f) * 1.25f, -0.5f },
            { 1.35f, 1.35f },
            m_ElapsedTime,
            { 0.95f, 0.45f, 0.25f, 0.90f });
        renderer2D.DrawQuad({ -2.0f, -1.4f, -1.0f }, { 1.25f, 1.25f }, { 0.25f, 0.90f, 0.45f, 0.85f });
    }

    Life::Camera* EditorShellOverlay::TryGetEditorCamera()
    {
        return GetApplication().GetService<Life::CameraManager>().GetCamera(m_EditorCameraName);
    }

    bool EditorShellOverlay::RenderSceneViewport(uint32_t width, uint32_t height)
    {
        if (!m_SceneViewport)
            return false;

        Life::Camera* editorCamera = TryGetEditorCamera();
        if (editorCamera == nullptr)
            return false;

        if (!m_SceneViewport->Resize(width, height))
            return false;

        editorCamera->SetAspectRatio(static_cast<float>(m_SceneViewport->GetWidth()) / static_cast<float>(m_SceneViewport->GetHeight()));

        if (!m_SceneViewport->BeginRender2D(*editorCamera))
            return false;

        DrawSceneViewportContent(m_SceneViewport->GetRenderer2D());
        m_SceneViewport->EndRender2D();

        return m_SceneViewport->Draw(static_cast<float>(width), static_cast<float>(height));
    }
}
