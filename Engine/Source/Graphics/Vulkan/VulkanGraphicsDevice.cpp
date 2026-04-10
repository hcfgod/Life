#include "Core/LifePCH.h"
#include "Graphics/Vulkan/VulkanGraphicsDevice.h"
#include "Core/Log.h"
#include "Core/Error.h"
#include "Core/Window.h"
#include "Platform/Vulkan/VulkanPlatformInterop.h"

#include <nvrhi/utils.h>

#include <array>
#include <cstring>
#include <string_view>

namespace Life
{
    void EnsureVulkanDispatchLoaderLinked() noexcept;
    void InitializeVulkanDispatchLoader(VkInstance instance) noexcept;
    void InitializeVulkanDispatchLoader(VkInstance instance, VkDevice device) noexcept;
    bool DiagnoseVulkanHppDispatcher(VkInstance instance, VkDevice device, char* outBuf, size_t bufSize) noexcept;

    namespace
    {
        class NvrhiMessageCallback final : public nvrhi::IMessageCallback
        {
        public:
            void message(nvrhi::MessageSeverity severity, const char* messageText) override
            {
                switch (severity)
                {
                case nvrhi::MessageSeverity::Info:
                    LOG_CORE_INFO("[NVRHI] {}", messageText);
                    break;
                case nvrhi::MessageSeverity::Warning:
                    LOG_CORE_WARN("[NVRHI] {}", messageText);
                    break;
                case nvrhi::MessageSeverity::Error:
                    LOG_CORE_ERROR("[NVRHI] {}", messageText);
                    break;
                case nvrhi::MessageSeverity::Fatal:
                    LOG_CORE_CRITICAL("[NVRHI] {}", messageText);
                    break;
                }
            }
        };

        NvrhiMessageCallback& GetNvrhiMessageCallback()
        {
            static NvrhiMessageCallback callback;
            return callback;
        }

        void VerifySemaphoreCreation(VkDevice device, VkSemaphoreType semaphoreType, const char* name)
        {
            VkSemaphoreTypeCreateInfo semaphoreTypeInfo{};
            semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            semaphoreTypeInfo.semaphoreType = semaphoreType;
            semaphoreTypeInfo.initialValue = 0;

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreInfo.pNext = semaphoreType == VK_SEMAPHORE_TYPE_BINARY ? nullptr : &semaphoreTypeInfo;

            VkSemaphore semaphore = VK_NULL_HANDLE;
            const VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
            if (result != VK_SUCCESS)
            {
                throw Error(
                    ErrorCode::GraphicsError,
                    std::string("Failed Vulkan ") + name + " semaphore preflight: " + nvrhi::vulkan::resultToString(result),
                    std::source_location::current(),
                    ErrorSeverity::Critical);
            }

            if (semaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(device, semaphore, nullptr);
        }

        void VerifyVulkanDeviceForNvrhi(VkDevice device)
        {
            VerifySemaphoreCreation(device, VK_SEMAPHORE_TYPE_BINARY, "binary");
            VerifySemaphoreCreation(device, VK_SEMAPHORE_TYPE_TIMELINE, "timeline");
        }

        bool ShouldIgnoreVulkanValidationMessage(const VkDebugUtilsMessengerCallbackDataEXT* callbackData)
        {
            if (callbackData == nullptr || callbackData->pMessage == nullptr)
                return false;

            const std::string_view message(callbackData->pMessage);

            return message.find("EOSOverlayVkLayer-Win64.json") != std::string_view::npos
                || message.find("loader_get_json:") != std::string_view::npos
                || message.find("windows_read_data_files_in_registry:") != std::string_view::npos;
        }

        VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            [[maybe_unused]] void* pUserData)
        {
            if (ShouldIgnoreVulkanValidationMessage(pCallbackData))
                return VK_FALSE;

            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                LOG_CORE_ERROR("[Vulkan Validation] {}", pCallbackData->pMessage);
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                LOG_CORE_WARN("[Vulkan Validation] {}", pCallbackData->pMessage);
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
                LOG_CORE_INFO("[Vulkan Validation] {}", pCallbackData->pMessage);

            return VK_FALSE;
        }
    }

    VulkanGraphicsDevice::VulkanGraphicsDevice(const GraphicsDeviceSpecification& spec, Window& window)
        : m_VSync(spec.VSync)
    {
        EnsureVulkanDispatchLoaderLinked();

        if (!Platform::VulkanInterop::WindowSupportsVulkan(window))
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Window does not support Vulkan.",
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        CreateInstance(spec.EnableValidation, window);
        CreateSurface(window);
        SelectPhysicalDevice();
        CreateLogicalDevice();
        CreateNvrhiDevice();
        CreateSwapchain();
        CreateFrameSynchronization();
        CreateSwapchainImages();
        CreateCommandList();

        LOG_CORE_INFO("Vulkan graphics device initialized successfully.");
    }

    VulkanGraphicsDevice::~VulkanGraphicsDevice()
    {
        if (m_Device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(m_Device);

        m_CommandList = nullptr;
        m_NvrhiSwapchainTextures.clear();

        if (m_NvrhiDevice)
        {
            m_NvrhiDevice = nullptr;
        }

        DestroyFrameSynchronization();
        DestroySwapchain();

        if (m_Surface != VK_NULL_HANDLE)
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

        vkb::destroy_device(m_VkbDevice);
        vkb::destroy_instance(m_VkbInstance);
    }

    void VulkanGraphicsDevice::CreateInstance(bool enableValidation, Window& window)
    {
        auto extensions = Platform::VulkanInterop::GetRequiredInstanceExtensions(window);

        vkb::InstanceBuilder builder;
        builder.set_app_name("Life Engine")
               .set_engine_name("Life")
               .require_api_version(1, 3, 0)
               .set_app_version(1, 0, 0);

        for (const char* ext : extensions)
            builder.enable_extension(ext);

        if (enableValidation)
        {
            builder.request_validation_layers()
                   .set_debug_callback(VulkanDebugCallback);
        }

        auto instanceResult = builder.build();
        if (!instanceResult)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to create Vulkan instance: " + instanceResult.error().message(),
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        m_VkbInstance = instanceResult.value();
        m_Instance = m_VkbInstance.instance;
        m_DebugMessenger = m_VkbInstance.debug_messenger;
        InitializeVulkanDispatchLoader(m_Instance);

        LOG_CORE_INFO("Vulkan instance created.");
    }

    void VulkanGraphicsDevice::CreateSurface(Window& window)
    {
        m_Surface = Platform::VulkanInterop::CreateSurface(window, m_Instance);
        if (m_Surface == VK_NULL_HANDLE)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to create Vulkan surface.",
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        LOG_CORE_INFO("Vulkan surface created.");
    }

    void VulkanGraphicsDevice::SelectPhysicalDevice()
    {
        vkb::PhysicalDeviceSelector selector(m_VkbInstance);
        selector.set_surface(m_Surface)
                .set_minimum_version(1, 3)
                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

        auto physResult = selector.select();
        if (!physResult)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to select a suitable Vulkan physical device: " + physResult.error().message(),
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        m_VkbPhysicalDevice = physResult.value();
        m_PhysicalDevice = m_VkbPhysicalDevice.physical_device;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
        LOG_CORE_INFO("Selected Vulkan device: {}", props.deviceName);
    }

    void VulkanGraphicsDevice::CreateLogicalDevice()
    {
        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.timelineSemaphore = VK_TRUE;

        vkb::DeviceBuilder deviceBuilder(m_VkbPhysicalDevice);
        deviceBuilder.add_pNext(&features12);
        auto deviceResult = deviceBuilder.build();
        if (!deviceResult)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to create Vulkan logical device: " + deviceResult.error().message(),
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        m_VkbDevice = deviceResult.value();
        m_Device = m_VkbDevice.device;

        auto graphicsQueueResult = m_VkbDevice.get_queue(vkb::QueueType::graphics);
        if (!graphicsQueueResult)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to get Vulkan graphics queue.",
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        m_GraphicsQueue = graphicsQueueResult.value();
        m_GraphicsQueueFamily = m_VkbDevice.get_queue_index(vkb::QueueType::graphics).value();

        auto presentQueueResult = m_VkbDevice.get_queue(vkb::QueueType::present);
        if (!presentQueueResult)
        {
            m_PresentQueue = m_GraphicsQueue;
            m_PresentQueueFamily = m_GraphicsQueueFamily;
        }
        else
        {
            m_PresentQueue = presentQueueResult.value();
            m_PresentQueueFamily = m_VkbDevice.get_queue_index(vkb::QueueType::present).value();
        }

        InitializeVulkanDispatchLoader(m_Instance, m_Device);

        LOG_CORE_INFO("Vulkan logical device created.");
    }

    void VulkanGraphicsDevice::CreateNvrhiDevice()
    {
        VerifyVulkanDeviceForNvrhi(m_Device);

        // Dispatcher diagnostic: test Vulkan-Hpp path from Engine-compiled code
        {
            char diagBuf[512]{};
            bool diagOk = DiagnoseVulkanHppDispatcher(m_Instance, m_Device, diagBuf, sizeof(diagBuf));
            LOG_CORE_INFO("{}", diagBuf);
            LOG_CORE_INFO("Vulkan-Hpp dispatcher test from Engine code: {}", diagOk ? "PASSED" : "FAILED");
        }

        // Collect enabled device extensions from vk-bootstrap as persistent C strings
        auto vkbDeviceExtensions = m_VkbPhysicalDevice.get_extensions();
        std::vector<const char*> deviceExtensionPtrs;
        deviceExtensionPtrs.reserve(vkbDeviceExtensions.size());
        for (const auto& ext : vkbDeviceExtensions)
            deviceExtensionPtrs.push_back(ext.c_str());

        nvrhi::vulkan::DeviceDesc deviceDesc{};
        deviceDesc.errorCB = &GetNvrhiMessageCallback();
        deviceDesc.instance = m_Instance;
        deviceDesc.physicalDevice = m_PhysicalDevice;
        deviceDesc.device = m_Device;
        deviceDesc.graphicsQueue = m_GraphicsQueue;
        deviceDesc.graphicsQueueIndex = static_cast<int>(m_GraphicsQueueFamily);
        deviceDesc.transferQueue = VK_NULL_HANDLE;
        deviceDesc.transferQueueIndex = -1;
        deviceDesc.computeQueue = VK_NULL_HANDLE;
        deviceDesc.computeQueueIndex = -1;
        deviceDesc.deviceExtensions = deviceExtensionPtrs.data();
        deviceDesc.numDeviceExtensions = deviceExtensionPtrs.size();

        m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);
        if (!m_NvrhiDevice)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to create NVRHI Vulkan device.",
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        LOG_CORE_INFO("NVRHI Vulkan device created.");
    }

    void VulkanGraphicsDevice::CreateSwapchain()
    {
        vkb::SwapchainBuilder swapchainBuilder(m_VkbDevice);
        swapchainBuilder.set_old_swapchain(m_Swapchain)
                        .set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });

        if (m_VSync)
            swapchainBuilder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
        else
            swapchainBuilder.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR);

        auto swapchainResult = swapchainBuilder.build();
        if (!swapchainResult)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to create Vulkan swapchain: " + swapchainResult.error().message(),
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        auto swapchain = swapchainResult.value();
        m_Swapchain = swapchain.swapchain;
        m_SwapchainWidth = swapchain.extent.width;
        m_SwapchainHeight = swapchain.extent.height;
        m_SwapchainImageFormat = swapchain.image_format;

        auto imagesResult = swapchain.get_images();
        if (!imagesResult)
        {
            throw Error(
                ErrorCode::GraphicsError,
                "Failed to get swapchain images.",
                std::source_location::current(),
                ErrorSeverity::Critical);
        }

        m_SwapchainImages = imagesResult.value();

        LOG_CORE_INFO("Vulkan swapchain created ({}x{}, {} images).", m_SwapchainWidth, m_SwapchainHeight, m_SwapchainImages.size());
    }

    void VulkanGraphicsDevice::CreateFrameSynchronization()
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        DestroyFrameSynchronization();

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        m_ImageAvailableSemaphores.assign(MaxFramesInFlight, VK_NULL_HANDLE);
        m_RenderFinishedSemaphores.assign(MaxFramesInFlight, VK_NULL_HANDLE);
        m_InFlightFences.assign(MaxFramesInFlight, VK_NULL_HANDLE);

        for (uint32_t i = 0; i < MaxFramesInFlight; ++i)
        {
            vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]);
            vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]);
            vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]);
        }
    }

    void VulkanGraphicsDevice::DestroyFrameSynchronization() noexcept
    {
        if (m_Device == VK_NULL_HANDLE)
        {
            m_ImageAvailableSemaphores.clear();
            m_RenderFinishedSemaphores.clear();
            m_InFlightFences.clear();
            return;
        }

        for (VkSemaphore& semaphore : m_ImageAvailableSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }

        for (VkSemaphore& semaphore : m_RenderFinishedSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }

        for (VkFence& fence : m_InFlightFences)
        {
            if (fence != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_Device, fence, nullptr);
                fence = VK_NULL_HANDLE;
            }
        }

        m_ImageAvailableSemaphores.clear();
        m_RenderFinishedSemaphores.clear();
        m_InFlightFences.clear();
    }

    void VulkanGraphicsDevice::DestroySwapchain()
    {
        m_SwapchainImages.clear();
        m_SwapchainImageFormat = VK_FORMAT_UNDEFINED;

        if (m_Swapchain != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanGraphicsDevice::CreateSwapchainImages()
    {
        m_NvrhiSwapchainTextures.clear();
        m_NvrhiSwapchainTextures.reserve(m_SwapchainImages.size());

        nvrhi::Format nvrhiFormat = nvrhi::Format::BGRA8_UNORM;

        for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
        {
            nvrhi::TextureDesc textureDesc;
            textureDesc.width = m_SwapchainWidth;
            textureDesc.height = m_SwapchainHeight;
            textureDesc.format = nvrhiFormat;
            textureDesc.debugName = "SwapchainImage_" + std::to_string(i);
            textureDesc.initialState = nvrhi::ResourceStates::Present;
            textureDesc.keepInitialState = true;
            textureDesc.isRenderTarget = true;

            nvrhi::TextureHandle texture = m_NvrhiDevice->createHandleForNativeTexture(
                nvrhi::ObjectTypes::VK_Image,
                nvrhi::Object(m_SwapchainImages[i]),
                textureDesc);

            m_NvrhiSwapchainTextures.push_back(texture);
        }
    }

    void VulkanGraphicsDevice::CreateCommandList()
    {
        nvrhi::CommandListParameters params;
        params.enableImmediateExecution = false;
        m_CommandList = m_NvrhiDevice->createCommandList(params);
    }

    bool VulkanGraphicsDevice::BeginFrame()
    {
        m_FrameActive = false;

        if (m_Device == VK_NULL_HANDLE || m_Swapchain == VK_NULL_HANDLE)
            return false;

        if (m_SwapchainWidth == 0 || m_SwapchainHeight == 0 || m_SwapchainImages.empty())
            return false;

        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(
            m_Device, m_Swapchain, UINT64_MAX,
            m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE,
            &m_CurrentImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            return false;
        }

        if (result == VK_NOT_READY || result == VK_TIMEOUT)
            return false;

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            LOG_CORE_ERROR("Failed to acquire swapchain image: {}", nvrhi::vulkan::resultToString(result));
            return false;
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

        m_NvrhiDevice->queueWaitForSemaphore(
            nvrhi::CommandQueue::Graphics,
            m_ImageAvailableSemaphores[m_CurrentFrame], 0);

        m_CommandList->open();
        m_FrameActive = true;

        return true;
    }

    void VulkanGraphicsDevice::Present()
    {
        m_CommandList->close();
        m_NvrhiDevice->executeCommandList(m_CommandList);

        m_NvrhiDevice->queueSignalSemaphore(
            nvrhi::CommandQueue::Graphics,
            m_RenderFinishedSemaphores[m_CurrentFrame], 0);

        m_NvrhiDevice->executeCommandLists(nullptr, 0);

        vkQueueSubmit(m_GraphicsQueue, 0, nullptr, m_InFlightFences[m_CurrentFrame]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            RecreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            LOG_CORE_ERROR("Failed to present swapchain image: {}", nvrhi::vulkan::resultToString(result));
        }

        m_FrameActive = false;
        m_CurrentFrame = (m_CurrentFrame + 1) % MaxFramesInFlight;
    }

    nvrhi::ITexture* VulkanGraphicsDevice::GetCurrentBackBuffer()
    {
        if (m_FrameActive && m_CurrentImageIndex < m_NvrhiSwapchainTextures.size())
            return m_NvrhiSwapchainTextures[m_CurrentImageIndex].Get();

        return nullptr;
    }

    nvrhi::IDevice* VulkanGraphicsDevice::GetNvrhiDevice()
    {
        return m_NvrhiDevice.Get();
    }

    nvrhi::ICommandList* VulkanGraphicsDevice::GetCurrentCommandList()
    {
        return m_FrameActive ? m_CommandList.Get() : nullptr;
    }

    void VulkanGraphicsDevice::RecreateSwapchain()
    {
        vkDeviceWaitIdle(m_Device);

        m_FrameActive = false;
        m_CurrentImageIndex = 0;
        m_CommandList = nullptr;
        m_NvrhiSwapchainTextures.clear();

        DestroySwapchain();
        CreateSwapchain();
        CreateSwapchainImages();
        CreateCommandList();

        LOG_CORE_INFO("Vulkan swapchain recreated ({}x{}).", m_SwapchainWidth, m_SwapchainHeight);
    }

    void VulkanGraphicsDevice::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        if (width == m_SwapchainWidth && height == m_SwapchainHeight)
            return;

        LOG_CORE_INFO("Vulkan swapchain resize requested from {}x{} to {}x{}.", m_SwapchainWidth, m_SwapchainHeight, width, height);
        RecreateSwapchain();
    }
}
