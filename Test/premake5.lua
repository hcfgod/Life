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

    links
    {
        "Engine",
        "ImGui",
        "VkBootstrap"
    }

    ConfigureGraphicsDefines()
    ConfigureSanitizers()
    ConfigureRuntimeSearchPaths()
    ConfigureSDL3Linking()
    ConfigureNVRHILinking()
    ConfigureVulkanLinking()
    ConfigureD3D12Linking()

    filter "system:linux"
        links { "Engine", "VkBootstrap" }

    filter {}

    ConfigureCommonProject()
