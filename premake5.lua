local function Trim(value)
     return (value:gsub("^%s+", ""):gsub("%s+$", ""))
 end

 local function NormalizeArchitecture(value)
     if value == nil then
         return nil
     end

     local normalizedValue = Trim(value:lower())
     if normalizedValue == "amd64" or normalizedValue == "x86_64" then
         return "x64"
     end

     if normalizedValue == "aarch64" or normalizedValue == "arm64" then
         return "arm64"
     end

     return normalizedValue
 end

 local function ResolveHostArchitecture()
     local environmentArchitecture = os.getenv("PROCESSOR_ARCHITEW6432") or os.getenv("PROCESSOR_ARCHITECTURE")
     local normalizedEnvironmentArchitecture = NormalizeArchitecture(environmentArchitecture)
     if normalizedEnvironmentArchitecture ~= nil then
         return normalizedEnvironmentArchitecture
     end

     local machineArchitecture = os.outputof("uname -m")
     local normalizedMachineArchitecture = NormalizeArchitecture(machineArchitecture)
     if normalizedMachineArchitecture ~= nil then
         return normalizedMachineArchitecture
     end

     return "x64"
 end

 local function ResolveTargetArchitecture()
     local requestedArchitecture = NormalizeArchitecture(_OPTIONS["arch"])
     if requestedArchitecture == "x64" or requestedArchitecture == "arm64" then
         return requestedArchitecture
     end

     local hostArchitecture = ResolveHostArchitecture()
     if hostArchitecture == "arm64" then
         return "arm64"
     end

     return "x64"
 end

 newoption
 {
     trigger = "arch",
     value = "ARCH",
     description = "Select the target architecture",
     allowed =
     {
         { "x64", "x64" },
         { "arm64", "arm64" }
     }
 }

 TargetArchitecture = ResolveTargetArchitecture()
 PremakeArchitecture = TargetArchitecture == "arm64" and "ARM64" or "x64"

 workspace "Life"
     architecture (PremakeArchitecture)
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

 outputdir = "%{cfg.system}-" .. TargetArchitecture .. "/%{cfg.buildcfg}"

 IncludeDir = {}
 IncludeDir["Engine"] = path.join(RootDir, "Engine/Include")
 IncludeDir["SDL3"] = path.join(RootDir, "Vendor/SDL3/include")
 IncludeDir["glm"] = path.join(RootDir, "Vendor/glm")
 IncludeDir["spdlog"] = path.join(RootDir, "Vendor/spdlog/include")
 IncludeDir["json"] = path.join(RootDir, "Vendor/json/include")
 IncludeDir["doctest"] = path.join(RootDir, "Vendor/doctest")
 IncludeDir["VulkanHeaders"] = path.join(RootDir, "Vendor/nvrhi/thirdparty/Vulkan-Headers/include")
 IncludeDir["DirectXHeaders"] = path.join(RootDir, "Vendor/nvrhi/thirdparty/DirectX-Headers/include/directx")
 IncludeDir["vk_bootstrap"] = path.join(RootDir, "Vendor/vk-bootstrap/src")

 local function GetSDLInstallPath(platformName, configuration, leaf)
     return path.join(RootDir, "Vendor/SDL3/Install/" .. platformName .. "/" .. TargetArchitecture .. "/" .. configuration .. "/" .. leaf)
 end

 LibraryDir = {}
 LibraryDir["SDL3_Windows_Debug"] = GetSDLInstallPath("windows", "Debug", "lib")
 LibraryDir["SDL3_Windows_Release"] = GetSDLInstallPath("windows", "Release", "lib")
 LibraryDir["SDL3_Linux_Debug"] = GetSDLInstallPath("linux", "Debug", "lib")
 LibraryDir["SDL3_Linux_Release"] = GetSDLInstallPath("linux", "Release", "lib")
 LibraryDir["SDL3_MacOS_Debug"] = GetSDLInstallPath("macos", "Debug", "lib")
 LibraryDir["SDL3_MacOS_Release"] = GetSDLInstallPath("macos", "Release", "lib")

 BinaryDir = {}
 BinaryDir["SDL3_Windows_Debug"] = GetSDLInstallPath("windows", "Debug", "bin")
 BinaryDir["SDL3_Windows_Release"] = GetSDLInstallPath("windows", "Release", "bin")
 BinaryDir["SDL3_Linux_Debug"] = GetSDLInstallPath("linux", "Debug", "lib")
 BinaryDir["SDL3_Linux_Release"] = GetSDLInstallPath("linux", "Release", "lib")
 BinaryDir["SDL3_MacOS_Debug"] = GetSDLInstallPath("macos", "Debug", "lib")
 BinaryDir["SDL3_MacOS_Release"] = GetSDLInstallPath("macos", "Release", "lib")

 BinaryFile = {}
 BinaryFile["SDL3_Windows_Debug"] = path.getabsolute(BinaryDir["SDL3_Windows_Debug"] .. "/SDL3.dll")
 BinaryFile["SDL3_Windows_Release"] = path.getabsolute(BinaryDir["SDL3_Windows_Release"] .. "/SDL3.dll")
 BinaryFile["SDL3_Linux_Debug"] = path.getabsolute(BinaryDir["SDL3_Linux_Debug"] .. "/libSDL3.so.0")
 BinaryFile["SDL3_Linux_Release"] = path.getabsolute(BinaryDir["SDL3_Linux_Release"] .. "/libSDL3.so.0")
 BinaryFile["SDL3_MacOS_Debug"] = path.getabsolute(BinaryDir["SDL3_MacOS_Debug"] .. "/libSDL3.0.dylib")
 BinaryFile["SDL3_MacOS_Release"] = path.getabsolute(BinaryDir["SDL3_MacOS_Release"] .. "/libSDL3.0.dylib")

 local function GetNVRHIInstallPath(platformName, configuration, leaf)
     return path.join(RootDir, "Vendor/nvrhi/Install/" .. platformName .. "/" .. TargetArchitecture .. "/" .. configuration .. "/" .. leaf)
 end

 NVRHILibDir = {}
 NVRHILibDir["Windows_Debug"] = GetNVRHIInstallPath("windows", "Debug", "lib")
 NVRHILibDir["Windows_Release"] = GetNVRHIInstallPath("windows", "Release", "lib")
 NVRHILibDir["Linux_Debug"] = GetNVRHIInstallPath("linux", "Debug", "lib")
 NVRHILibDir["Linux_Release"] = GetNVRHIInstallPath("linux", "Release", "lib")
 NVRHILibDir["MacOS_Debug"] = GetNVRHIInstallPath("macos", "Debug", "lib")
 NVRHILibDir["MacOS_Release"] = GetNVRHIInstallPath("macos", "Release", "lib")

 -- Resolve Vulkan SDK path for Windows linking
 local function ResolveVulkanSDKPath()
     local vulkanSDK = os.getenv("VULKAN_SDK")
     if vulkanSDK ~= nil and vulkanSDK ~= "" then
         return vulkanSDK
     end
     local localPath = path.join(RootDir, "Vendor/VulkanSDK")
     if os.isdir(localPath) then
         local entries = os.matchdirs(localPath .. "/*")
         if #entries > 0 then
             local sdkRoot = entries[#entries]

             if os.host() == "macosx" then
                 local macSdkRoot = path.join(sdkRoot, "macOS")
                 if os.isdir(macSdkRoot) then
                     return macSdkRoot
                 end
             elseif os.host() == "linux" then
                 local linuxSdkRoot = path.join(sdkRoot, "x86_64")
                 if os.isdir(linuxSdkRoot) then
                     return linuxSdkRoot
                 end
             end

             return sdkRoot
         end
     end
     return nil
 end

 VulkanSDKPath = ResolveVulkanSDKPath()

function SetupProject()
    language "C++"
    cppdialect "C++20"
    targetdir (path.join(RootDir, "Build/" .. outputdir .. "/%{prj.name}"))
    objdir (path.join(RootDir, "Build/Intermediate/" .. outputdir .. "/%{prj.name}"))
end

function ConfigureProjectPCH(header, source)
    pchheader (header)
    pchsource (source)
    forceincludes { header }
end

function UseEngineIncludeDirs(extraIncludeDirs)
    local includeDirs =
    {
        IncludeDir["Engine"]
    }

    local externalIncludeDirs =
    {
        IncludeDir["SDL3"],
        IncludeDir["glm"],
        IncludeDir["spdlog"],
        IncludeDir["json"],
        IncludeDir["VulkanHeaders"],
        IncludeDir["vk_bootstrap"]
    }

    if extraIncludeDirs ~= nil then
        for _, includeDir in ipairs(extraIncludeDirs) do
            table.insert(externalIncludeDirs, includeDir)
        end
    end

    includedirs(includeDirs)
    externalincludedirs(externalIncludeDirs)

    filter { "system:windows", "configurations:Debug" }
        externalincludedirs { GetNVRHIInstallPath("windows", "Debug", "include") }

    filter { "system:windows", "configurations:Release" }
        externalincludedirs { GetNVRHIInstallPath("windows", "Release", "include") }

    filter { "system:windows", "configurations:Dist" }
        externalincludedirs { GetNVRHIInstallPath("windows", "Release", "include") }

    filter { "system:linux", "configurations:Debug" }
        externalincludedirs { GetNVRHIInstallPath("linux", "Debug", "include") }

    filter { "system:linux", "configurations:Release" }
        externalincludedirs { GetNVRHIInstallPath("linux", "Release", "include") }

    filter { "system:linux", "configurations:Dist" }
        externalincludedirs { GetNVRHIInstallPath("linux", "Release", "include") }

    filter { "system:macosx", "configurations:Debug" }
        externalincludedirs { GetNVRHIInstallPath("macos", "Debug", "include") }

    filter { "system:macosx", "configurations:Release" }
        externalincludedirs { GetNVRHIInstallPath("macos", "Release", "include") }

    filter { "system:macosx", "configurations:Dist" }
        externalincludedirs { GetNVRHIInstallPath("macos", "Release", "include") }

    filter {}
end

function ConfigureCommonProject()
    filter "system:windows"
        systemversion "latest"
        links { "Dbghelp" }

    filter { "system:windows", "action:vs*" }
        buildoptions { "/analyze:external-" }
    filter "system:linux"
        pic "On"

    filter { "system:macosx", "architecture:ARM64" }
        buildoptions { "-arch", "arm64" }
        linkoptions { "-arch", "arm64" }

    filter { "system:macosx", "architecture:x64" }
        buildoptions { "-arch", "x86_64" }
        linkoptions { "-arch", "x86_64" }

    filter {}

    filter { "action:vs*", "configurations:Debug" }
        staticruntime "off"
        runtime "Debug"

    filter { "action:vs*", "configurations:Release" }
        staticruntime "off"
        runtime "Release"

    filter { "action:vs*", "configurations:Dist" }
        staticruntime "off"
        runtime "Release"

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"

    filter "configurations:Dist"
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
        postbuildcommands { 'cp -a "' .. BinaryDir["SDL3_Linux_Debug"] .. '"/libSDL3.so* "%{cfg.targetdir}"' }

    filter { "system:linux", "configurations:Release" }
        libdirs { LibraryDir["SDL3_Linux_Release"] }
        links { "SDL3" }
        postbuildcommands { 'cp -a "' .. BinaryDir["SDL3_Linux_Release"] .. '"/libSDL3.so* "%{cfg.targetdir}"' }

    filter { "system:linux", "configurations:Dist" }
        libdirs { LibraryDir["SDL3_Linux_Release"] }
        links { "SDL3" }
        postbuildcommands { 'cp -a "' .. BinaryDir["SDL3_Linux_Release"] .. '"/libSDL3.so* "%{cfg.targetdir}"' }

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

function ConfigureGraphicsDefines()
    filter "system:windows"
        defines { "LIFE_GRAPHICS_VULKAN", "LIFE_GRAPHICS_D3D12", "VK_USE_PLATFORM_WIN32_KHR", "NOMINMAX" }

    filter "system:linux"
        defines { "LIFE_GRAPHICS_VULKAN" }

    filter "system:macosx"
        defines { "LIFE_GRAPHICS_VULKAN", "VK_USE_PLATFORM_METAL_EXT" }

    filter {}
end

function ConfigureNVRHILinking()
    filter { "system:windows", "configurations:Debug" }
        libdirs { NVRHILibDir["Windows_Debug"] }
        links { "nvrhi", "nvrhi_vk", "nvrhi_d3d12" }

    filter { "system:windows", "configurations:Release" }
        libdirs { NVRHILibDir["Windows_Release"] }
        links { "nvrhi", "nvrhi_vk", "nvrhi_d3d12" }

    filter { "system:windows", "configurations:Dist" }
        libdirs { NVRHILibDir["Windows_Release"] }
        links { "nvrhi", "nvrhi_vk", "nvrhi_d3d12" }

    filter { "system:linux", "configurations:Debug" }
        libdirs { NVRHILibDir["Linux_Debug"] }
        links { "nvrhi_vk", "nvrhi", "Engine", "VkBootstrap" }

    filter { "system:linux", "configurations:Release" }
        libdirs { NVRHILibDir["Linux_Release"] }
        links { "nvrhi_vk", "nvrhi", "Engine", "VkBootstrap" }

    filter { "system:linux", "configurations:Dist" }
        libdirs { NVRHILibDir["Linux_Release"] }
        links { "nvrhi_vk", "nvrhi", "Engine", "VkBootstrap" }

    filter { "system:macosx", "configurations:Debug" }
        libdirs { NVRHILibDir["MacOS_Debug"] }
        links { "nvrhi_vk", "nvrhi" }

    filter { "system:macosx", "configurations:Release" }
        libdirs { NVRHILibDir["MacOS_Release"] }
        links { "nvrhi_vk", "nvrhi" }

    filter { "system:macosx", "configurations:Dist" }
        libdirs { NVRHILibDir["MacOS_Release"] }
        links { "nvrhi_vk", "nvrhi" }

    filter {}
end

function ConfigureVulkanLinking()
    filter "system:windows"
        if VulkanSDKPath ~= nil then
            libdirs { path.join(VulkanSDKPath, "Lib") }
        end
        links { "vulkan-1" }

    filter "system:linux"
        links { "vulkan" }

    filter "system:macosx"
        links { "vulkan" }
        if VulkanSDKPath ~= nil then
            libdirs { path.join(VulkanSDKPath, "lib") }
        end

    filter {}
end

function ConfigureD3D12Linking()
    filter "system:windows"
        links { "d3d12", "dxgi", "dxguid" }

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

group "Dependencies"
include "Vendor/vk-bootstrap"
group ""

include "Engine"
include "Runtime"
include "Test"
