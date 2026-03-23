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

    UseEngineIncludeDirs()
    ConfigureProjectPCH("Core/LifePCH.h", "Source/Core/LifePCH.cpp")
    ConfigureSanitizers()
    ConfigureCommonProject()
