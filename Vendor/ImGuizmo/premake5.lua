project "ImGuizmo"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "ImGuizmo.h",
        "ImGuizmo.cpp"
    }

    defines
    {
        "IMGUI_DEFINE_MATH_OPERATORS"
    }

    disablewarnings
    {
        "6001",
        "6255",
        "6263"
    }

    externalincludedirs
    {
        IncludeDir["imgui"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
