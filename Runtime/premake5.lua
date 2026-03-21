project "Runtime"
    location "."
    kind "ConsoleApp"

    SetupProject()

    files
    {
        "Source/**.h",
        "Source/**.hpp",
        "Source/**.cpp"
    }

    UseEngineIncludeDirs()

    links
    {
        "Engine"
    }

    ConfigureApplicationEntrypoints()
    ConfigureSanitizers()
    ConfigureRuntimeSearchPaths()
    ConfigureSDL3Linking()
    ConfigureCommonProject()
