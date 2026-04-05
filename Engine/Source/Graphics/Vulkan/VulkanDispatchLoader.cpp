#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <cstdio>
#include <exception>
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
            try
            {
                static VulkanHppDynamicLoader dynamicLoader;
                return dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
            }
            catch (...)
            {
                return nullptr;
            }
        }
    }

    void EnsureVulkanDispatchLoaderLinked() noexcept
    {
        auto* dispatcher = &VULKAN_HPP_DEFAULT_DISPATCHER;
        (void)dispatcher;
        if (const PFN_vkGetInstanceProcAddr getInstanceProcAddr = GetVulkanGetInstanceProcAddr())
        {
            try
            {
                VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstanceProcAddr);
            }
            catch (...)
            {
            }
        }
    }

    void InitializeVulkanDispatchLoader(VkInstance instance) noexcept
    {
        if (instance == VK_NULL_HANDLE)
            return;

        if (const PFN_vkGetInstanceProcAddr getInstanceProcAddr = GetVulkanGetInstanceProcAddr())
        {
            try
            {
                VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, getInstanceProcAddr);
            }
            catch (...)
            {
            }
        }
    } 

    void InitializeVulkanDispatchLoader(VkInstance instance, VkDevice device) noexcept
    {
        if (instance == VK_NULL_HANDLE)
            return;

        if (const PFN_vkGetInstanceProcAddr getInstanceProcAddr = GetVulkanGetInstanceProcAddr())
        {
            try
            {
                VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, getInstanceProcAddr, device);
            }
            catch (...)
            {
            }
        }
    }

    bool DiagnoseVulkanHppDispatcher(VkInstance instance, VkDevice device, char* outBuf, size_t bufSize) noexcept
    {
        (void)instance;

        try
        {
            auto& d = VULKAN_HPP_DEFAULT_DISPATCHER;
            if (outBuf != nullptr && bufSize > 0)
            {
                std::snprintf(outBuf, bufSize,
                    "DispatcherDiag: addr=%p sizeof=%zu vkCreateSemaphore=%p vkDestroySemaphore=%p vkGetDeviceProcAddr=%p",
                    static_cast<void*>(&d),
                    sizeof(d),
                    reinterpret_cast<void*>(d.vkCreateSemaphore),
                    reinterpret_cast<void*>(d.vkDestroySemaphore),
                    reinterpret_cast<void*>(d.vkGetDeviceProcAddr));
            }

            if (!d.vkCreateSemaphore || !d.vkDestroySemaphore)
                return false;

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
        catch (const std::exception& ex)
        {
            if (outBuf != nullptr && bufSize > 0)
                std::snprintf(outBuf, bufSize, "DispatcherDiag: exception=%s", ex.what());
            return false;
        }
        catch (...)
        {
            if (outBuf != nullptr && bufSize > 0)
                std::snprintf(outBuf, bufSize, "DispatcherDiag: unknown exception");
            return false;
        }
    }
}