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
    ConfigureSanitizers()
    ConfigureCommonProject()
