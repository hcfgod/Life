workspace "Life"
    architecture "x64"
    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

    startproject "Runtime"

    filter "action:vs*"
        buildoptions { "/utf-8" }

    filter {}

RootDir = os.getcwd()

outputdir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

IncludeDir = {}
IncludeDir["Engine"] = path.join(RootDir, "Engine/Include")
IncludeDir["SDL3"] = path.join(RootDir, "Vendor/SDL3/include")
IncludeDir["spdlog"] = path.join(RootDir, "Vendor/spdlog/include")
IncludeDir["json"] = path.join(RootDir, "Vendor/json/include")
IncludeDir["doctest"] = path.join(RootDir, "Vendor/doctest")

LibraryDir = {}
LibraryDir["SDL3_Windows_Debug"] = path.join(RootDir, "Vendor/SDL3/Install/windows/x64/Debug/lib")
LibraryDir["SDL3_Windows_Release"] = path.join(RootDir, "Vendor/SDL3/Install/windows/x64/Release/lib")
LibraryDir["SDL3_Linux_Debug"] = path.join(RootDir, "Vendor/SDL3/Install/linux/x64/Debug/lib")
LibraryDir["SDL3_Linux_Release"] = path.join(RootDir, "Vendor/SDL3/Install/linux/x64/Release/lib")
LibraryDir["SDL3_MacOS_Debug"] = path.join(RootDir, "Vendor/SDL3/Install/macos/x64/Debug/lib")
LibraryDir["SDL3_MacOS_Release"] = path.join(RootDir, "Vendor/SDL3/Install/macos/x64/Release/lib")

BinaryDir = {}
BinaryDir["SDL3_Windows_Debug"] = path.join(RootDir, "Vendor/SDL3/Install/windows/x64/Debug/bin")
BinaryDir["SDL3_Windows_Release"] = path.join(RootDir, "Vendor/SDL3/Install/windows/x64/Release/bin")
BinaryDir["SDL3_Linux_Debug"] = path.join(RootDir, "Vendor/SDL3/Install/linux/x64/Debug/lib")
BinaryDir["SDL3_Linux_Release"] = path.join(RootDir, "Vendor/SDL3/Install/linux/x64/Release/lib")
BinaryDir["SDL3_MacOS_Debug"] = path.join(RootDir, "Vendor/SDL3/Install/macos/x64/Debug/lib")
BinaryDir["SDL3_MacOS_Release"] = path.join(RootDir, "Vendor/SDL3/Install/macos/x64/Release/lib")

BinaryFile = {}
BinaryFile["SDL3_Windows_Debug"] = path.getabsolute(BinaryDir["SDL3_Windows_Debug"] .. "/SDL3.dll")
BinaryFile["SDL3_Windows_Release"] = path.getabsolute(BinaryDir["SDL3_Windows_Release"] .. "/SDL3.dll")
BinaryFile["SDL3_Linux_Debug"] = path.getabsolute(BinaryDir["SDL3_Linux_Debug"] .. "/libSDL3.so")
BinaryFile["SDL3_Linux_Release"] = path.getabsolute(BinaryDir["SDL3_Linux_Release"] .. "/libSDL3.so")
BinaryFile["SDL3_MacOS_Debug"] = path.getabsolute(BinaryDir["SDL3_MacOS_Debug"] .. "/libSDL3.dylib")
BinaryFile["SDL3_MacOS_Release"] = path.getabsolute(BinaryDir["SDL3_MacOS_Release"] .. "/libSDL3.dylib")

function SetupProject()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    targetdir (path.join(RootDir, "Build/" .. outputdir .. "/%{prj.name}"))
    objdir (path.join(RootDir, "Build/Intermediate/" .. outputdir .. "/%{prj.name}"))
end

function UseEngineIncludeDirs(extraIncludeDirs)
    local includeDirs =
    {
        IncludeDir["Engine"],
        IncludeDir["SDL3"],
        IncludeDir["spdlog"],
        IncludeDir["json"]
    }

    if extraIncludeDirs ~= nil then
        for _, includeDir in ipairs(extraIncludeDirs) do
            table.insert(includeDirs, includeDir)
        end
    end

    includedirs(includeDirs)
end

function ConfigureCommonProject()
    filter "system:windows"
        systemversion "latest"

    filter "system:linux"
        pic "On"

    filter {}

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter "configurations:Dist"
        runtime "Release"
        optimize "Full"

    filter {}
end

function ConfigureSDL3Linking()
    filter { "system:windows", "configurations:Debug" }
        libdirs { LibraryDir["SDL3_Windows_Debug"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_Windows_Debug"] .. '" "%{cfg.targetdir}"' }

    filter { "system:windows", "configurations:Release" }
        libdirs { LibraryDir["SDL3_Windows_Release"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_Windows_Release"] .. '" "%{cfg.targetdir}"' }

    filter { "system:windows", "configurations:Dist" }
        libdirs { LibraryDir["SDL3_Windows_Release"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_Windows_Release"] .. '" "%{cfg.targetdir}"' }

    filter { "system:linux", "configurations:Debug" }
        libdirs { LibraryDir["SDL3_Linux_Debug"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_Linux_Debug"] .. '" "%{cfg.targetdir}"' }

    filter { "system:linux", "configurations:Release" }
        libdirs { LibraryDir["SDL3_Linux_Release"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_Linux_Release"] .. '" "%{cfg.targetdir}"' }

    filter { "system:linux", "configurations:Dist" }
        libdirs { LibraryDir["SDL3_Linux_Release"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_Linux_Release"] .. '" "%{cfg.targetdir}"' }

    filter { "system:macosx", "configurations:Debug" }
        libdirs { LibraryDir["SDL3_MacOS_Debug"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_MacOS_Debug"] .. '" "%{cfg.targetdir}"' }

    filter { "system:macosx", "configurations:Release" }
        libdirs { LibraryDir["SDL3_MacOS_Release"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_MacOS_Release"] .. '" "%{cfg.targetdir}"' }

    filter { "system:macosx", "configurations:Dist" }
        libdirs { LibraryDir["SDL3_MacOS_Release"] }
        links { "SDL3" }
        postbuildcommands { '{COPY} "' .. BinaryFile["SDL3_MacOS_Release"] .. '" "%{cfg.targetdir}"' }

    filter {}
end

function ConfigureApplicationEntrypoints()
    filter "system:windows"
        defines { "LIFE_ENABLE_ENTRYPOINT" }

    filter "system:linux"
        defines { "LIFE_ENABLE_ENTRYPOINT" }

    filter "system:macosx"
        defines { "LIFE_ENABLE_ENTRYPOINT" }

    filter "system:android"
        defines { "LIFE_ENABLE_SDL_ENTRYPOINT" }

    filter "system:ios"
        defines { "LIFE_ENABLE_SDL_ENTRYPOINT" }

    filter {}
end

include "Engine"
include "Runtime"
include "Test"
