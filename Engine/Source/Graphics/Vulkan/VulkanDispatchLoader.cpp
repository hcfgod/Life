#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Life
{
    void EnsureVulkanDispatchLoaderLinked() noexcept
    {
        auto* dispatcher = &VULKAN_HPP_DEFAULT_DISPATCHER;
        (void)dispatcher;
    }
}
