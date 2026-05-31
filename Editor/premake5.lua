project "Editor"
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
        "Include",
        IncludeDir["ImGuizmo"]
    }

    links
    {
        "Engine",
        "StbImage",
        "ImGui",
        "ImGuizmo",
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
