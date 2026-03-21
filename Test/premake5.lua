project "Test"
    location "."
    kind "ConsoleApp"

    SetupProject()

    files
    {
        "Source/**.h",
        "Source/**.hpp",
        "Source/**.cpp"
    }

    UseEngineIncludeDirs({ IncludeDir["doctest"] })

    links
    {
        "Engine"
    }

    ConfigureSDL3Linking()
    ConfigureCommonProject()
