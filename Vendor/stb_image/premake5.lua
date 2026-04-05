project "StbImage"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "stb_image.h",
        "stb_image_source.h",
        "stb_image_impl.cpp"
    }

    includedirs
    {
        "."
    }

    externalincludedirs
    {
        IncludeDir["SDL3"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
