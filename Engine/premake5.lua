project "Engine"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "Source/**.h",
        "Source/**.hpp",
        "Source/**.cpp",
        "Include/**.h",
        "Include/**.hpp"
    }

    links
    {
        "ImGui",
        "VkBootstrap"
    }

    UseEngineIncludeDirs()

    includedirs
    {
        "Source"
    }
    ConfigureProjectPCH("Core/LifePCH.h", "Source/Core/LifePCH.cpp")
    ConfigureGraphicsDefines()
    ConfigureSanitizers()
    ConfigureCommonProject()

    filter "system:windows"
        externalincludedirs { IncludeDir["DirectXHeaders"] }

    filter {}
