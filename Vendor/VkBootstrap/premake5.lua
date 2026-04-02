project "VkBootstrap"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "../vk-bootstrap/src/VkBootstrap.h",
        "../vk-bootstrap/src/VkBootstrap.cpp",
        "../vk-bootstrap/src/VkBootstrapDispatch.h",
        "../vk-bootstrap/src/VkBootstrapFeatureChain.h",
        "../vk-bootstrap/src/VkBootstrapFeatureChain.inl"
    }

    includedirs
    {
        "../vk-bootstrap/src"
    }

    externalincludedirs
    {
        IncludeDir["VulkanHeaders"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
