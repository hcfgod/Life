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

newoption
{
    trigger = "asan",
    description = "Enable AddressSanitizer instrumentation for supported targets"
}

newoption
{
    trigger = "ubsan",
    description = "Enable UndefinedBehaviorSanitizer instrumentation for supported targets"
}

newoption
{
    trigger = "tsan",
    description = "Enable ThreadSanitizer instrumentation for supported targets"
}

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
        IncludeDir["Engine"]
    }

    local externalIncludeDirs =
    {
        IncludeDir["SDL3"],
        IncludeDir["spdlog"],
        IncludeDir["json"]
    }

    if extraIncludeDirs ~= nil then
        for _, includeDir in ipairs(extraIncludeDirs) do
            table.insert(externalIncludeDirs, includeDir)
        end
    end

    includedirs(includeDirs)
    externalincludedirs(externalIncludeDirs)
end

function ConfigureCommonProject()
    filter "system:windows"
        systemversion "latest"

    filter { "system:windows", "action:vs*" }
        buildoptions { "/analyze:external-" }

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

function ConfigureSanitizers()
    filter { "system:linux", "options:asan" }
        buildoptions { "-fsanitize=address", "-fno-omit-frame-pointer" }
        linkoptions { "-fsanitize=address" }

    filter { "system:linux", "options:ubsan" }
        buildoptions { "-fsanitize=undefined", "-fno-omit-frame-pointer" }
        linkoptions { "-fsanitize=undefined" }

    filter { "system:linux", "options:tsan" }
        buildoptions { "-fsanitize=thread", "-fno-omit-frame-pointer" }
        linkoptions { "-fsanitize=thread" }

    filter { "system:macosx", "options:asan" }
        buildoptions { "-fsanitize=address", "-fno-omit-frame-pointer" }
        linkoptions { "-fsanitize=address" }

    filter { "system:macosx", "options:ubsan" }
        buildoptions { "-fsanitize=undefined", "-fno-omit-frame-pointer" }
        linkoptions { "-fsanitize=undefined" }

    filter { "system:macosx", "options:tsan" }
        buildoptions { "-fsanitize=thread", "-fno-omit-frame-pointer" }
        linkoptions { "-fsanitize=thread" }

    filter { "options:asan" }
        symbols "On"

    filter { "options:ubsan" }
        symbols "On"

    filter { "options:tsan" }
        symbols "On"

    filter {}
end

function ConfigureRuntimeSearchPaths()
    filter "system:linux"
        runpathdirs { "$ORIGIN" }

    filter "system:macosx"
        runpathdirs { "@executable_path" }

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
