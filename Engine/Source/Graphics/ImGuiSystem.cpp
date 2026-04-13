#include "Core/LifePCH.h"
#include "Graphics/ImGuiSystem.h"

#include "Graphics/Detail/ImGuiRendererBackend.h"

#include "Core/Events/EventBase.h"
#include "Core/Log.h"
#include "Core/Window.h"
#include "Graphics/GraphicsDevice.h"

namespace Life
{
    namespace
    {
#if LIFE_HAS_IMGUI
        bool InitializePlatformBackend(GraphicsBackend backend, SDL_Window* window)
        {
            switch (backend)
            {
            case GraphicsBackend::Vulkan:
                return ImGui_ImplSDL3_InitForVulkan(window);
            case GraphicsBackend::D3D12:
                return ImGui_ImplSDL3_InitForD3D(window);
            case GraphicsBackend::None:
            default:
                return ImGui_ImplSDL3_InitForOther(window);
            }
        }

        void ConfigureImGuiStyle()
        {
            ImGui::StyleColorsDark();

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            io.ConfigWindowsMoveFromTitleBarOnly = true;
            io.IniFilename = nullptr;

            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 6.0f;
            style.ChildRounding = 6.0f;
            style.FrameRounding = 5.0f;
            style.PopupRounding = 5.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 5.0f;
            style.TabRounding = 4.0f;
            style.WindowBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f;
            style.TabBorderSize = 0.0f;
            style.WindowPadding = ImVec2(10.0f, 10.0f);
            style.FramePadding = ImVec2(8.0f, 5.0f);
            style.ItemSpacing = ImVec2(8.0f, 6.0f);
            style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
            style.CellPadding = ImVec2(6.0f, 4.0f);

            ImVec4* colors = style.Colors;
            colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.105f, 0.11f, 1.0f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.145f, 0.15f, 1.0f);
            colors[ImGuiCol_Header] = ImVec4(0.20f, 0.205f, 0.21f, 1.0f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.285f, 0.29f, 1.0f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.245f, 0.25f, 1.0f);
            colors[ImGuiCol_Button] = ImVec4(0.20f, 0.205f, 0.21f, 1.0f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.285f, 0.29f, 1.0f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.245f, 0.25f, 1.0f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.165f, 0.17f, 1.0f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.245f, 0.25f, 1.0f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.205f, 0.21f, 1.0f);
            colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.18f, 1.0f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.46f, 0.69f, 1.0f);
            colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.34f, 0.52f, 1.0f);
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.24f, 0.34f, 1.0f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.17f, 0.22f, 1.0f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.46f, 0.68f, 0.98f, 1.0f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.39f, 0.60f, 0.92f, 1.0f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.46f, 0.68f, 0.98f, 1.0f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.34f, 0.52f, 0.25f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.46f, 0.68f, 0.98f, 0.70f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.68f, 0.98f, 0.95f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.38f, 0.46f, 0.69f, 0.78f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.46f, 0.68f, 0.98f, 1.0f);
            colors[ImGuiCol_DockingPreview] = ImVec4(0.46f, 0.68f, 0.98f, 0.40f);
            colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.085f, 0.09f, 1.0f);
        }

        bool ShouldCaptureInputEvent(const Event& event)
        {
            const ImGuiIO& io = ImGui::GetIO();
            switch (event.GetEventType())
            {
            case EventType::KeyPressed:
            case EventType::KeyReleased:
                return io.WantCaptureKeyboard;
            case EventType::MouseMoved:
            case EventType::MouseScrolled:
            case EventType::MouseButtonPressed:
            case EventType::MouseButtonReleased:
                return io.WantCaptureMouse;
            default:
                return false;
            }
        }
#endif
    }

    struct ImGuiSystem::Impl
    {
        SDL_Window* SdlWindow = nullptr;
#if LIFE_HAS_IMGUI
        Scope<Detail::ImGuiRendererBackend> RendererBackend;
#endif
    };

    ImGuiSystem::ImGuiSystem(Window& window, GraphicsDevice* graphicsDevice)
        : m_Window(window)
        , m_GraphicsDevice(graphicsDevice)
        , m_Impl(CreateScope<Impl>())
    {
        m_Backend = m_GraphicsDevice != nullptr ? m_GraphicsDevice->GetBackend() : GraphicsBackend::None;
    }

    ImGuiSystem::~ImGuiSystem() noexcept
    {
        Shutdown();
    }

    void ImGuiSystem::Initialize()
    {
        if (m_Initialized)
            return;

#if !LIFE_HAS_IMGUI
        LOG_CORE_WARN("ImGuiSystem is unavailable because Dear ImGui headers were not present during this build.");
        m_Available = false;
        return;
#else
        if (m_GraphicsDevice == nullptr)
        {
            m_Available = false;
            return;
        }

        m_Impl->SdlWindow = static_cast<SDL_Window*>(m_Window.GetNativeHandle());
        if (m_Impl->SdlWindow == nullptr)
        {
            LOG_CORE_WARN("ImGuiSystem is unavailable because the platform window does not expose an SDL window handle.");
            m_Available = false;
            return;
        }

        m_Backend = m_GraphicsDevice->GetBackend();
        m_Impl->RendererBackend = Detail::CreateImGuiRendererBackend(*m_GraphicsDevice);
        if (!m_Impl->RendererBackend)
        {
            LOG_CORE_WARN("ImGuiSystem does not yet support the active graphics backend.");
            m_Available = false;
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ConfigureImGuiStyle();

        if (!InitializePlatformBackend(m_Backend, m_Impl->SdlWindow))
        {
            LOG_CORE_ERROR("Failed to initialize the ImGui SDL3 platform backend.");
            ImGui::DestroyContext();
            m_Impl->RendererBackend.reset();
            m_Available = false;
            return;
        }

        if (!m_Impl->RendererBackend->Initialize())
        {
            LOG_CORE_ERROR("Failed to initialize the ImGui renderer backend.");
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            m_Impl->RendererBackend.reset();
            m_Available = false;
            return;
        }

        m_Available = true;
        m_Initialized = true;
        LOG_CORE_INFO("ImGuiSystem initialized for backend {}.", static_cast<int>(m_Backend));
#endif
    }

    void ImGuiSystem::Shutdown() noexcept
    {
#if LIFE_HAS_IMGUI
        if (m_Initialized)
        {
            if (m_FrameActive)
            {
                ImGui::EndFrame();
                m_FrameActive = false;
            }

            if (m_Impl && m_Impl->RendererBackend)
            {
                m_Impl->RendererBackend->Shutdown();
                m_Impl->RendererBackend.reset();
            }

            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        }
        else if (m_Impl)
        {
            m_Impl->RendererBackend.reset();
        }
#endif

        m_Initialized = false;
        m_Available = false;
        m_FrameActive = false;
        m_Backend = m_GraphicsDevice != nullptr ? m_GraphicsDevice->GetBackend() : GraphicsBackend::None;
    }

    void ImGuiSystem::BeginFrame()
    {
#if LIFE_HAS_IMGUI
        if (!m_Initialized || !m_Available || m_FrameActive || m_Impl == nullptr || !m_Impl->RendererBackend)
            return;

        m_Impl->RendererBackend->NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        m_FrameActive = true;
#endif
    }

    void ImGuiSystem::Render()
    {
#if LIFE_HAS_IMGUI
        if (!m_Initialized || !m_Available || !m_FrameActive || m_Impl == nullptr || !m_Impl->RendererBackend)
            return;

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == nullptr || drawData->CmdListsCount == 0)
        {
            static bool loggedMissingDrawData = false;
            if (!loggedMissingDrawData)
            {
                LOG_CORE_WARN("ImGuiSystem::Render produced no draw commands.");
                loggedMissingDrawData = true;
            }
        }

        m_Impl->RendererBackend->RenderDrawData(drawData);
        m_FrameActive = false;
#endif
    }

    void ImGuiSystem::OnSdlEvent(const SDL_Event& event)
    {
#if LIFE_HAS_IMGUI
        if (!m_Initialized || !m_Available)
            return;

        ImGui_ImplSDL3_ProcessEvent(&event);
#else
        (void)event;
#endif
    }

    void ImGuiSystem::CaptureEvent(Event& event)
    {
#if LIFE_HAS_IMGUI
        if (!m_Initialized || !m_Available)
            return;

        if (ShouldCaptureInputEvent(event))
            event.Accept();
#else
        (void)event;
#endif
    }

    bool ImGuiSystem::DrawImage(TextureResource& texture, float width, float height)
    {
#if LIFE_HAS_IMGUI
        if (!m_Initialized || !m_Available || m_Impl == nullptr || !m_Impl->RendererBackend)
            return false;

        if (width <= 0.0f || height <= 0.0f)
            return false;

        void* textureHandle = m_Impl->RendererBackend->GetTextureHandle(texture);
        if (textureHandle == nullptr)
            return false;

        ImGui::Image(ImTextureRef(textureHandle), ImVec2(width, height), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        return true;
#else
        (void)texture;
        (void)width;
        (void)height;
        return false;
#endif
    }

    void ImGuiSystem::ReleaseTextureHandle(TextureResource& texture) noexcept
    {
#if LIFE_HAS_IMGUI
        if (!m_Initialized || m_Impl == nullptr || !m_Impl->RendererBackend)
            return;

        m_Impl->RendererBackend->ReleaseTextureHandle(texture);
#else
        (void)texture;
#endif
    }

    bool ImGuiSystem::WantsKeyboardCapture() const noexcept
    {
#if LIFE_HAS_IMGUI
        return m_Initialized && m_Available && ImGui::GetIO().WantCaptureKeyboard;
#else
        return false;
#endif
    }

    bool ImGuiSystem::WantsMouseCapture() const noexcept
    {
#if LIFE_HAS_IMGUI
        return m_Initialized && m_Available && ImGui::GetIO().WantCaptureMouse;
#else
        return false;
#endif
    }
}
