#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Life
{
    namespace
    {
#if VK_HEADER_VERSION >= 301
        using VulkanHppDynamicLoader = vk::detail::DynamicLoader;
#else
        using VulkanHppDynamicLoader = vk::DynamicLoader;
#endif

        PFN_vkGetInstanceProcAddr GetVulkanGetInstanceProcAddr() noexcept
        {
            static VulkanHppDynamicLoader dynamicLoader;
            return dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        }
    }

    void EnsureVulkanDispatchLoaderLinked() noexcept
    {
        auto* dispatcher = &VULKAN_HPP_DEFAULT_DISPATCHER;
        (void)dispatcher;
        if (const PFN_vkGetInstanceProcAddr getInstanceProcAddr = GetVulkanGetInstanceProcAddr())
            VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstanceProcAddr);
    }

    void InitializeVulkanDispatchLoader(VkInstance instance) noexcept
    {
        if (instance == VK_NULL_HANDLE)
            return;

        if (const PFN_vkGetInstanceProcAddr getInstanceProcAddr = GetVulkanGetInstanceProcAddr())
            VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, getInstanceProcAddr);
    } 

    void InitializeVulkanDispatchLoader(VkInstance instance, VkDevice device) noexcept
    {
        if (instance == VK_NULL_HANDLE)
            return;

        if (const PFN_vkGetInstanceProcAddr getInstanceProcAddr = GetVulkanGetInstanceProcAddr())
            VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, getInstanceProcAddr, device);
    }

    bool DiagnoseVulkanHppDispatcher(VkInstance instance, VkDevice device, char* outBuf, size_t bufSize) noexcept
    {
        auto& d = VULKAN_HPP_DEFAULT_DISPATCHER;
        std::snprintf(outBuf, bufSize,
            "DispatcherDiag: addr=%p sizeof=%zu vkCreateSemaphore=%p vkDestroySemaphore=%p vkGetDeviceProcAddr=%p",
            static_cast<void*>(&d),
            sizeof(d),
            reinterpret_cast<void*>(d.vkCreateSemaphore),
            reinterpret_cast<void*>(d.vkDestroySemaphore),
            reinterpret_cast<void*>(d.vkGetDeviceProcAddr));

        if (!d.vkCreateSemaphore || !d.vkDestroySemaphore)
            return false;

        // Mirror exactly what NVRHI Queue::Queue does, but from Engine-compiled code
        vk::Device vkDevice(device);
        auto semaphoreTypeInfo = vk::SemaphoreTypeCreateInfo()
            .setSemaphoreType(vk::SemaphoreType::eTimeline);
        auto semaphoreInfo = vk::SemaphoreCreateInfo()
            .setPNext(&semaphoreTypeInfo);

        vk::Semaphore testSem = vkDevice.createSemaphore(semaphoreInfo, nullptr);
        if (testSem)
            vkDevice.destroySemaphore(testSem, nullptr);
        return true;
    }
}