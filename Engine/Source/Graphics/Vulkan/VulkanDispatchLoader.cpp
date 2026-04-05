#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#if !defined(_WIN32)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace Life
{
    void EnsureVulkanDispatchLoaderLinked() noexcept
    {
#if !defined(_WIN32)
        auto* dispatcher = &VULKAN_HPP_DEFAULT_DISPATCHER;
        (void)dispatcher;
#endif
    }
}