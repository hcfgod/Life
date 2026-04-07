#include "Core/LifePCH.h"
#include "Graphics/ImGuiSystem.h"

#include "Core/Events/Event.h"
#include "Core/Log.h"
#include "Core/Window.h"
#include "Graphics/GraphicsDevice.h"
#include "Graphics/TextureResource.h"
#include "Graphics/Vulkan/VulkanGraphicsDevice.h"

#include <nvrhi/nvrhi.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <unordered_map>
#include <utility>

#if __has_include(<imgui.h>) && __has_include(<backends/imgui_impl_sdl3.h>)
#define LIFE_HAS_IMGUI 1
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#else
#define LIFE_HAS_IMGUI 0
#endif

#if LIFE_HAS_IMGUI && __has_include(<backends/imgui_impl_vulkan.h>)
#define LIFE_HAS_IMGUI_VULKAN 1
#include <backends/imgui_impl_vulkan.h>
#else
#define LIFE_HAS_IMGUI_VULKAN 0
#endif

namespace Life
{
    namespace
    {
#if LIFE_HAS_IMGUI
        class ImGuiRendererBackend
        {
        public:
            virtual ~ImGuiRendererBackend() = default;

            virtual bool Initialize() = 0;
            virtual void Shutdown() noexcept = 0;
            virtual void NewFrame() = 0;
            virtual void RenderDrawData(ImDrawData* drawData) = 0;
            virtual void* GetTextureHandle(TextureResource& texture) = 0;
            virtual void ReleaseTextureHandle(TextureResource& texture) noexcept = 0;
        };

#if LIFE_HAS_IMGUI_VULKAN
        class ImGuiVulkanBackend final : public ImGuiRendererBackend
        {
        public:
            explicit ImGuiVulkanBackend(VulkanGraphicsDevice& graphicsDevice)
                : m_GraphicsDevice(graphicsDevice)
            {
            }

            bool Initialize() override
            {
                static constexpr std::array<VkDescriptorPoolSize, 11> DescriptorPoolSizes =
                {
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 64 },
                    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 64 }
                };

                VkDescriptorPoolCreateInfo descriptorPoolInfo{};
                descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                descriptorPoolInfo.maxSets = 1024;
                descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(DescriptorPoolSizes.size());
                descriptorPoolInfo.pPoolSizes = DescriptorPoolSizes.data();

                const VkResult descriptorPoolResult = vkCreateDescriptorPool(
                    m_GraphicsDevice.GetDevice(),
                    &descriptorPoolInfo,
                    nullptr,
                    &m_DescriptorPool);
                if (descriptorPoolResult != VK_SUCCESS)
                {
                    LOG_CORE_ERROR("Failed to create ImGui Vulkan descriptor pool: {}", static_cast<int>(descriptorPoolResult));
                    return false;
                }

                m_ColorAttachmentFormat = m_GraphicsDevice.GetSwapchainImageFormat();
                if (m_ColorAttachmentFormat == VK_FORMAT_UNDEFINED)
                    m_ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;

                ImGui_ImplVulkan_InitInfo initInfo{};
                initInfo.ApiVersion = m_GraphicsDevice.GetApiVersion();
                initInfo.Instance = m_GraphicsDevice.GetInstance();
                initInfo.PhysicalDevice = m_GraphicsDevice.GetPhysicalDevice();
                initInfo.Device = m_GraphicsDevice.GetDevice();
                initInfo.QueueFamily = m_GraphicsDevice.GetGraphicsQueueFamilyIndex();
                initInfo.Queue = m_GraphicsDevice.GetGraphicsQueue();
                initInfo.DescriptorPool = m_DescriptorPool;
                initInfo.MinImageCount = std::max(2u, m_GraphicsDevice.GetFramesInFlight());
                initInfo.ImageCount = std::max(initInfo.MinImageCount, m_GraphicsDevice.GetSwapchainImageCount());
                initInfo.PipelineCache = VK_NULL_HANDLE;
                initInfo.UseDynamicRendering = true;
                initInfo.PipelineInfoMain.Subpass = 0;
                initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
                initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {};
                initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
                initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
                initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_ColorAttachmentFormat;
                initInfo.PipelineInfoForViewports = initInfo.PipelineInfoMain;
                initInfo.CheckVkResultFn = &CheckVkResult;
                initInfo.MinAllocationSize = static_cast<VkDeviceSize>(1024) * static_cast<VkDeviceSize>(1024);

                if (!ImGui_ImplVulkan_Init(&initInfo))
                {
                    LOG_CORE_ERROR("Failed to initialize ImGui Vulkan backend.");
                    vkDestroyDescriptorPool(m_GraphicsDevice.GetDevice(), m_DescriptorPool, nullptr);
                    m_DescriptorPool = VK_NULL_HANDLE;
                    return false;
                }

                VkSamplerCreateInfo samplerInfo{};
                samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                samplerInfo.magFilter = VK_FILTER_LINEAR;
                samplerInfo.minFilter = VK_FILTER_LINEAR;
                samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                samplerInfo.minLod = 0.0f;
                samplerInfo.maxLod = 1.0f;
                samplerInfo.maxAnisotropy = 1.0f;

                const VkResult samplerResult = vkCreateSampler(
                    m_GraphicsDevice.GetDevice(),
                    &samplerInfo,
                    nullptr,
                    &m_TextureSampler);
                if (samplerResult != VK_SUCCESS)
                {
                    LOG_CORE_ERROR("Failed to create ImGui Vulkan texture sampler: {}", static_cast<int>(samplerResult));
                    ImGui_ImplVulkan_Shutdown();
                    vkDestroyDescriptorPool(m_GraphicsDevice.GetDevice(), m_DescriptorPool, nullptr);
                    m_DescriptorPool = VK_NULL_HANDLE;
                    return false;
                }

                m_Initialized = true;
                return true;
            }

            void Shutdown() noexcept override
            {
                if (!m_Initialized)
                    return;

                for (auto& [nativeTexture, binding] : m_TextureBindings)
                {
                    (void)nativeTexture;
                    if (binding.DescriptorSet != VK_NULL_HANDLE)
                        ImGui_ImplVulkan_RemoveTexture(binding.DescriptorSet);
                }
                m_TextureBindings.clear();

                if (m_TextureSampler != VK_NULL_HANDLE)
                {
                    vkDestroySampler(m_GraphicsDevice.GetDevice(), m_TextureSampler, nullptr);
                    m_TextureSampler = VK_NULL_HANDLE;
                }

                ImGui_ImplVulkan_Shutdown();
                if (m_DescriptorPool != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(m_GraphicsDevice.GetDevice(), m_DescriptorPool, nullptr);
                    m_DescriptorPool = VK_NULL_HANDLE;
                }

                m_Initialized = false;
            }

            void NewFrame() override
            {
                ImGui_ImplVulkan_NewFrame();
            }

            void RenderDrawData(ImDrawData* drawData) override
            {
                if (drawData == nullptr || drawData->CmdListsCount == 0)
                    return;

                if (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f)
                    return;

                nvrhi::ICommandList* commandList = m_GraphicsDevice.GetCurrentCommandList();
                nvrhi::ITexture* backBuffer = m_GraphicsDevice.GetCurrentBackBuffer();
                if (commandList == nullptr || backBuffer == nullptr)
                    return;

                commandList->setTextureState(backBuffer, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
                commandList->commitBarriers();

                VkCommandBuffer vkCommandBuffer = commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
                if (vkCommandBuffer == VK_NULL_HANDLE)
                    return;

                const VkImageView backBufferImageView = backBuffer->getNativeView(nvrhi::ObjectTypes::VK_ImageView);
                if (backBufferImageView == VK_NULL_HANDLE)
                    return;

                VkRenderingAttachmentInfo colorAttachmentInfo{};
                colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                colorAttachmentInfo.imageView = backBufferImageView;
                colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                VkRenderingInfo renderingInfo{};
                renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                renderingInfo.renderArea.offset = { 0, 0 };
                renderingInfo.renderArea.extent =
                {
                    m_GraphicsDevice.GetBackBufferWidth(),
                    m_GraphicsDevice.GetBackBufferHeight()
                };
                renderingInfo.layerCount = 1;
                renderingInfo.colorAttachmentCount = 1;
                renderingInfo.pColorAttachments = &colorAttachmentInfo;

                vkCmdBeginRendering(vkCommandBuffer, &renderingInfo);

                ImGui_ImplVulkan_RenderDrawData(drawData, vkCommandBuffer);

                vkCmdEndRendering(vkCommandBuffer);

                commandList->setTextureState(backBuffer, nvrhi::AllSubresources, nvrhi::ResourceStates::Present);
                commandList->commitBarriers();
            }

            void* GetTextureHandle(TextureResource& texture) override
            {
                if (!m_Initialized || m_TextureSampler == VK_NULL_HANDLE)
                    return nullptr;

                nvrhi::ITexture* nativeTexture = texture.GetNativeHandle();
                if (nativeTexture == nullptr)
                    return nullptr;

                if (auto existing = m_TextureBindings.find(nativeTexture); existing != m_TextureBindings.end())
                    return reinterpret_cast<void*>(existing->second.DescriptorSet);

                VkImageView imageView = nativeTexture->getNativeView(nvrhi::ObjectTypes::VK_ImageView);
                if (imageView == VK_NULL_HANDLE)
                    return nullptr;

                const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
                    m_TextureSampler,
                    imageView,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                if (descriptorSet == VK_NULL_HANDLE)
                    return nullptr;

                m_TextureBindings.emplace(nativeTexture, TextureBinding{ descriptorSet });
                return reinterpret_cast<void*>(descriptorSet);
            }

            void ReleaseTextureHandle(TextureResource& texture) noexcept override
            {
                nvrhi::ITexture* nativeTexture = texture.GetNativeHandle();
                if (nativeTexture == nullptr)
                    return;

                const auto it = m_TextureBindings.find(nativeTexture);
                if (it == m_TextureBindings.end())
                    return;

                if (it->second.DescriptorSet != VK_NULL_HANDLE)
                    ImGui_ImplVulkan_RemoveTexture(it->second.DescriptorSet);

                m_TextureBindings.erase(it);
            }

        private:
            struct TextureBinding
            {
                VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
            };

            static void CheckVkResult(VkResult result)
            {
                if (result == VK_SUCCESS)
                    return;

                LOG_CORE_ERROR("ImGui Vulkan backend reported VkResult {}.", static_cast<int>(result));
            }

            VulkanGraphicsDevice& m_GraphicsDevice;
            VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
            VkFormat m_ColorAttachmentFormat = VK_FORMAT_UNDEFINED;
            VkSampler m_TextureSampler = VK_NULL_HANDLE;
            std::unordered_map<nvrhi::ITexture*, TextureBinding> m_TextureBindings;
            bool m_Initialized = false;
        };
#endif

        Scope<ImGuiRendererBackend> CreateRendererBackend(GraphicsDevice& graphicsDevice)
        {
            if (graphicsDevice.GetBackend() == GraphicsBackend::Vulkan)
            {
#if LIFE_HAS_IMGUI_VULKAN
                if (auto* vulkanGraphicsDevice = dynamic_cast<VulkanGraphicsDevice*>(&graphicsDevice))
                    return CreateScope<ImGuiVulkanBackend>(*vulkanGraphicsDevice);
#endif
            }

            return nullptr;
        }

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
            io.IniFilename = "imgui.ini";

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
        Scope<ImGuiRendererBackend> RendererBackend;
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
        m_Impl->RendererBackend = CreateRendererBackend(*m_GraphicsDevice);
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
