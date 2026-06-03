project "StbImage"
    location (path.join(RootDir, "BuildScripts/GeneratedProjects/StbImage"))
    kind "StaticLib"

    SetupProject()

    files
    {
        path.join(RootDir, "Vendor/stb_image/stb_image.h"),
        path.join(RootDir, "Vendor/stb_image/stb_image_source.h"),
        path.join(RootDir, "Vendor/stb_image/stb_image_impl.cpp")
    }

    includedirs
    {
        path.join(RootDir, "Vendor/stb_image")
    }

    externalincludedirs
    {
        IncludeDir["SDL3"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
