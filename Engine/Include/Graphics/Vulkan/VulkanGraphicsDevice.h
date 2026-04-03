#pragma once

#include "Graphics/GraphicsDevice.h"

#include <vulkan/vulkan.h>

#include <nvrhi/vulkan.h>
#include <VkBootstrap.h>

#include <vector>

namespace Life
{
    class VulkanGraphicsDevice final : public GraphicsDevice
    {
    public:
        explicit VulkanGraphicsDevice(const GraphicsDeviceSpecification& spec, Window& window);
        ~VulkanGraphicsDevice() override;

        bool BeginFrame() override;
        void Present() override;

        nvrhi::ITexture* GetCurrentBackBuffer() override;
        nvrhi::IDevice* GetNvrhiDevice() override;
        nvrhi::ICommandList* GetCurrentCommandList() override;

        uint32_t GetBackBufferWidth() const override { return m_SwapchainWidth; }
        uint32_t GetBackBufferHeight() const override { return m_SwapchainHeight; }
        GraphicsBackend GetBackend() const override { return GraphicsBackend::Vulkan; }

        void Resize(uint32_t width, uint32_t height) override;

    private:
        void CreateInstance(bool enableValidation, Window& window);
        void SelectPhysicalDevice();
        void CreateLogicalDevice();
        void CreateSurface(Window& window);
        void CreateSwapchain();
        void DestroySwapchain();
        void RecreateSwapchain();
        void CreateNvrhiDevice();
        void CreateSwapchainImages();
        void CreateCommandList();

        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        uint32_t m_GraphicsQueueFamily = 0;
        VkQueue m_PresentQueue = VK_NULL_HANDLE;
        uint32_t m_PresentQueueFamily = 0;
        VkFormat m_SwapchainFormat = VK_FORMAT_UNDEFINED;

        uint32_t m_SwapchainWidth = 0;
        uint32_t m_SwapchainHeight = 0;
        uint32_t m_CurrentImageIndex = 0;
        bool m_VSync = true;

        std::vector<VkImage> m_SwapchainImages;
        std::vector<nvrhi::TextureHandle> m_NvrhiSwapchainTextures;

        nvrhi::vulkan::DeviceHandle m_NvrhiDevice;
        nvrhi::CommandListHandle m_CommandList;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;
        uint32_t m_CurrentFrame = 0;
        bool m_FrameActive = false;
        static constexpr uint32_t MaxFramesInFlight = 2;

        vkb::Instance m_VkbInstance;
        vkb::Device m_VkbDevice;
    };
}
