#include "Core/LifePCH.h"
#include "Graphics/Detail/ImGuiVulkanRendererBackend.h"

#include "Core/Log.h"
#include "Graphics/TextureResource.h"
#include "Graphics/Vulkan/VulkanGraphicsDevice.h"

#include <nvrhi/nvrhi.h>

#include <algorithm>
#include <array>
#include <unordered_map>

#if LIFE_HAS_IMGUI_VULKAN
#include <backends/imgui_impl_vulkan.h>
#endif

namespace Life::Detail
{
#if LIFE_HAS_IMGUI_VULKAN
    namespace
    {
        void CheckVkResult(VkResult result)
        {
            if (result == VK_SUCCESS)
                return;

            LOG_CORE_ERROR("ImGui Vulkan backend reported VkResult {}.", static_cast<int>(result));
        }
    }

    struct ImGuiVulkanRendererBackend::Impl
    {
        struct TextureBinding
        {
            VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
        };

        VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
        VkFormat ColorAttachmentFormat = VK_FORMAT_UNDEFINED;
        VkSampler TextureSampler = VK_NULL_HANDLE;
        std::unordered_map<nvrhi::ITexture*, TextureBinding> TextureBindings;
    };

    ImGuiVulkanRendererBackend::ImGuiVulkanRendererBackend(VulkanGraphicsDevice& graphicsDevice)
        : m_GraphicsDevice(graphicsDevice)
        , m_Impl(CreateScope<Impl>())
    {
    }

    ImGuiVulkanRendererBackend::~ImGuiVulkanRendererBackend() = default;

    bool ImGuiVulkanRendererBackend::Initialize()
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
            &m_Impl->DescriptorPool);
        if (descriptorPoolResult != VK_SUCCESS)
        {
            LOG_CORE_ERROR("Failed to create ImGui Vulkan descriptor pool: {}", static_cast<int>(descriptorPoolResult));
            return false;
        }

        m_Impl->ColorAttachmentFormat = m_GraphicsDevice.GetSwapchainImageFormat();
        if (m_Impl->ColorAttachmentFormat == VK_FORMAT_UNDEFINED)
            m_Impl->ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = m_GraphicsDevice.GetApiVersion();
        initInfo.Instance = m_GraphicsDevice.GetInstance();
        initInfo.PhysicalDevice = m_GraphicsDevice.GetPhysicalDevice();
        initInfo.Device = m_GraphicsDevice.GetDevice();
        initInfo.QueueFamily = m_GraphicsDevice.GetGraphicsQueueFamilyIndex();
        initInfo.Queue = m_GraphicsDevice.GetGraphicsQueue();
        initInfo.DescriptorPool = m_Impl->DescriptorPool;
        initInfo.MinImageCount = std::max(2u, m_GraphicsDevice.GetFramesInFlight());
        initInfo.ImageCount = std::max(initInfo.MinImageCount, m_GraphicsDevice.GetSwapchainImageCount());
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {};
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_Impl->ColorAttachmentFormat;
        initInfo.PipelineInfoForViewports = initInfo.PipelineInfoMain;
        initInfo.CheckVkResultFn = &CheckVkResult;
        initInfo.MinAllocationSize = static_cast<VkDeviceSize>(1024) * static_cast<VkDeviceSize>(1024);

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            LOG_CORE_ERROR("Failed to initialize ImGui Vulkan backend.");
            vkDestroyDescriptorPool(m_GraphicsDevice.GetDevice(), m_Impl->DescriptorPool, nullptr);
            m_Impl->DescriptorPool = VK_NULL_HANDLE;
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
            &m_Impl->TextureSampler);
        if (samplerResult != VK_SUCCESS)
        {
            LOG_CORE_ERROR("Failed to create ImGui Vulkan texture sampler: {}", static_cast<int>(samplerResult));
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(m_GraphicsDevice.GetDevice(), m_Impl->DescriptorPool, nullptr);
            m_Impl->DescriptorPool = VK_NULL_HANDLE;
            return false;
        }

        m_Initialized = true;
        return true;
    }

    void ImGuiVulkanRendererBackend::Shutdown() noexcept
    {
        if (!m_Initialized)
            return;

        for (auto& [nativeTexture, binding] : m_Impl->TextureBindings)
        {
            (void)nativeTexture;
            if (binding.DescriptorSet != VK_NULL_HANDLE)
                ImGui_ImplVulkan_RemoveTexture(binding.DescriptorSet);
        }
        m_Impl->TextureBindings.clear();

        if (m_Impl->TextureSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(m_GraphicsDevice.GetDevice(), m_Impl->TextureSampler, nullptr);
            m_Impl->TextureSampler = VK_NULL_HANDLE;
        }

        ImGui_ImplVulkan_Shutdown();
        if (m_Impl->DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_GraphicsDevice.GetDevice(), m_Impl->DescriptorPool, nullptr);
            m_Impl->DescriptorPool = VK_NULL_HANDLE;
        }

        m_Initialized = false;
    }

    void ImGuiVulkanRendererBackend::NewFrame()
    {
        ImGui_ImplVulkan_NewFrame();
    }

    void ImGuiVulkanRendererBackend::RenderDrawData(ImDrawData* drawData)
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

    void* ImGuiVulkanRendererBackend::GetTextureHandle(TextureResource& texture)
    {
        if (!m_Initialized || m_Impl->TextureSampler == VK_NULL_HANDLE)
            return nullptr;

        nvrhi::ITexture* nativeTexture = texture.GetNativeHandle();
        if (nativeTexture == nullptr)
            return nullptr;

        if (auto existing = m_Impl->TextureBindings.find(nativeTexture); existing != m_Impl->TextureBindings.end())
            return reinterpret_cast<void*>(existing->second.DescriptorSet);

        const VkImageView imageView = nativeTexture->getNativeView(nvrhi::ObjectTypes::VK_ImageView);
        if (imageView == VK_NULL_HANDLE)
            return nullptr;

        const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
            m_Impl->TextureSampler,
            imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (descriptorSet == VK_NULL_HANDLE)
            return nullptr;

        m_Impl->TextureBindings.emplace(nativeTexture, Impl::TextureBinding{ descriptorSet });
        return reinterpret_cast<void*>(descriptorSet);
    }

    void ImGuiVulkanRendererBackend::ReleaseTextureHandle(TextureResource& texture) noexcept
    {
        nvrhi::ITexture* nativeTexture = texture.GetNativeHandle();
        if (nativeTexture == nullptr)
            return;

        const auto it = m_Impl->TextureBindings.find(nativeTexture);
        if (it == m_Impl->TextureBindings.end())
            return;

        if (it->second.DescriptorSet != VK_NULL_HANDLE)
            ImGui_ImplVulkan_RemoveTexture(it->second.DescriptorSet);

        m_Impl->TextureBindings.erase(it);
    }
#endif
}
