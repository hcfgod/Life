project "VkBootstrap"
    location (path.join(RootDir, "BuildScripts/GeneratedProjects/VkBootstrap"))
    kind "StaticLib"

    SetupProject()

    files
    {
        path.join(RootDir, "Vendor/vk-bootstrap/src/VkBootstrap.h"),
        path.join(RootDir, "Vendor/vk-bootstrap/src/VkBootstrap.cpp"),
        path.join(RootDir, "Vendor/vk-bootstrap/src/VkBootstrapDispatch.h"),
        path.join(RootDir, "Vendor/vk-bootstrap/src/VkBootstrapFeatureChain.h"),
        path.join(RootDir, "Vendor/vk-bootstrap/src/VkBootstrapFeatureChain.inl")
    }

    includedirs
    {
        path.join(RootDir, "Vendor/vk-bootstrap/src")
    }

    externalincludedirs
    {
        IncludeDir["VulkanHeaders"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
