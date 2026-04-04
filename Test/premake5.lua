project "Test"
    location "."
    kind "ConsoleApp"

    SetupProject()

    files
    {
        "Include/**.h",
        "Include/**.hpp",
        "Source/**.h",
        "Source/**.hpp",
        "Source/**.cpp"
    }

    UseEngineIncludeDirs({ IncludeDir["doctest"] })

    includedirs
    {
        "Include"
    }

    filter "system:linux"
        files { "../Engine/Source/Graphics/Vulkan/VulkanDispatchLoader.cpp" }

    filter {}

    links
    {
        "Engine",
        "VkBootstrap"
    }

    ConfigureGraphicsDefines()
    ConfigureSanitizers()
    ConfigureRuntimeSearchPaths()
    ConfigureSDL3Linking()
    ConfigureNVRHILinking()
    ConfigureVulkanLinking()
    ConfigureD3D12Linking()
    ConfigureCommonProject()
