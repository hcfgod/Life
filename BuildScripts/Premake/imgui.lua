project "ImGui"
    location (path.join(RootDir, "BuildScripts/GeneratedProjects/ImGui"))
    kind "StaticLib"

    SetupProject()

    files
    {
        path.join(RootDir, "Vendor/imgui/imgui.h"),
        path.join(RootDir, "Vendor/imgui/imgui_internal.h"),
        path.join(RootDir, "Vendor/imgui/imconfig.h"),
        path.join(RootDir, "Vendor/imgui/imstb_rectpack.h"),
        path.join(RootDir, "Vendor/imgui/imstb_textedit.h"),
        path.join(RootDir, "Vendor/imgui/imstb_truetype.h"),
        path.join(RootDir, "Vendor/imgui/imgui.cpp"),
        path.join(RootDir, "Vendor/imgui/imgui_draw.cpp"),
        path.join(RootDir, "Vendor/imgui/imgui_tables.cpp"),
        path.join(RootDir, "Vendor/imgui/imgui_widgets.cpp"),
        path.join(RootDir, "Vendor/imgui/backends/imgui_impl_sdl3.h"),
        path.join(RootDir, "Vendor/imgui/backends/imgui_impl_sdl3.cpp"),
        path.join(RootDir, "Vendor/imgui/backends/imgui_impl_vulkan.h"),
        path.join(RootDir, "Vendor/imgui/backends/imgui_impl_vulkan.cpp")
    }

    includedirs
    {
        path.join(RootDir, "Vendor/imgui"),
        path.join(RootDir, "Vendor/imgui/backends"),
        IncludeDir["VulkanHeaders"]
    }

    externalincludedirs
    {
        IncludeDir["SDL3"]
    }

    if VulkanSDKPath ~= nil then
        filter { "system:windows" }
            includedirs { path.join(VulkanSDKPath, "Include") }
    end

    filter {}

    ConfigureSanitizers()
    ConfigureCommonProject()
