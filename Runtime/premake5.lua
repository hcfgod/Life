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

    if VulkanSDKPath ~= nil then
        filter "system:windows"
            postbuildcommands
            {
                '{MKDIR} "%{cfg.targetdir}/Assets/Shaders"',
                '"' .. path.join(VulkanSDKPath, "Bin/glslangValidator.exe") .. '" -V "' .. path.join(RootDir, "Assets/Shaders/Renderer2D.vert") .. '" -o "%{cfg.targetdir}/Assets/Shaders/Renderer2D.vert.spv"',
                '"' .. path.join(VulkanSDKPath, "Bin/glslangValidator.exe") .. '" -V "' .. path.join(RootDir, "Assets/Shaders/Renderer2D.frag") .. '" -o "%{cfg.targetdir}/Assets/Shaders/Renderer2D.frag.spv"'
            }

        filter {}
    end

    ConfigureApplicationEntrypoints()
    ConfigureGraphicsDefines()
    ConfigureSanitizers()
    ConfigureRuntimeSearchPaths()
    ConfigureSDL3Linking()
    ConfigureNVRHILinking()
    ConfigureVulkanLinking()
    ConfigureD3D12Linking()

    ConfigureCommonProject()
