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
        "Engine"
    }

    ConfigureSanitizers()
    ConfigureRuntimeSearchPaths()
    ConfigureSDL3Linking()
    ConfigureCommonProject()
