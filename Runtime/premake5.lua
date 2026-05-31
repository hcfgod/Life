project "Runtime"
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

    UseEngineIncludeDirs()

    includedirs
    {
        "Include"
    }

    links
    {
        "Engine",
        "StbImage",
        "ImGui",
        "VkBootstrap"
    }

    ConfigureApplicationEntrypoints()
    ConfigureGraphicsDefines()
    ConfigureSanitizers()
    ConfigureRenderer2DShaderPostBuild()
    ConfigureRuntimeSearchPaths()
    ConfigureSDL3Linking()
    ConfigureNVRHILinking()
    ConfigureVulkanLinking()
    ConfigureD3D12Linking()

    ConfigureCommonProject()
