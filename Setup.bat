@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PREMAKE_VERSION=5.0.0-beta2"
set "CMAKE_VERSION=4.3.0"
set "VULKAN_SDK_VERSION=1.4.304.1"
set "CMAKE_CMD="
set "TARGET_ARCH="
set "SDL_CMAKE_ARCHITECTURE="
set "PREMAKE_ACTION_ARG=%~1"
if not "%~1"=="" shift
set "PREMAKE_EXTRA_ARGS=%*"

for %%A in (%PREMAKE_EXTRA_ARGS%) do (
    set "CURRENT_ARG=%%~A"
    call :capture_arch_arg
    if errorlevel 1 goto :error
)

if not defined TARGET_ARCH set "TARGET_ARCH=x64"
if /i "%TARGET_ARCH%"=="amd64" set "TARGET_ARCH=x64"
if /i "%TARGET_ARCH%"=="x86_64" set "TARGET_ARCH=x64"
if /i "%TARGET_ARCH%"=="aarch64" set "TARGET_ARCH=arm64"
if /i not "%TARGET_ARCH%"=="x64" if /i not "%TARGET_ARCH%"=="arm64" (
    echo [Setup] Unsupported target architecture: %TARGET_ARCH%
    goto :error
)

pushd "%~dp0" >nul || goto :error

call :needs_bootstrap
if errorlevel 1 goto :error
if /i "%NEEDS_BOOTSTRAP%"=="1" (
    echo [Setup] Bootstrap state not found. Running bootstrap...
    call "Scripts\BootstrapRepo.bat"
    if errorlevel 1 goto :error
)

echo [Setup] Initializing git submodules...
git submodule init
if errorlevel 1 goto :error

echo [Setup] Updating submodules recursively...
git submodule update --init --recursive
if errorlevel 1 goto :error

call :ensure_vk_bootstrap_premake
if errorlevel 1 goto :error

call :ensure_imgui_premake
if errorlevel 1 goto :error

call :resolve_premake "%PREMAKE_ACTION_ARG%"
if errorlevel 1 goto :error

call :resolve_cmake
if errorlevel 1 goto :error

call :build_sdl
if errorlevel 1 goto :error

call :resolve_vulkan_sdk
if errorlevel 1 goto :error

call :build_nvrhi
if errorlevel 1 goto :error

echo [Setup] Resolved Premake command: "%PREMAKE_CMD%"
echo [Setup] Generating project files with Premake (%PREMAKE_ACTION%)...
call "%PREMAKE_CMD%" %PREMAKE_ACTION% %PREMAKE_EXTRA_ARGS%
if errorlevel 1 goto :error

echo [Setup] Dependencies, SDL3, NVRHI, and project files are ready.
popd >nul
exit /b 0

:ensure_vk_bootstrap_premake
if not exist "Vendor\vk-bootstrap\" (
    echo [Setup] Vendor\vk-bootstrap was not found after submodule sync.
    exit /b 1
)

> "Vendor\vk-bootstrap\premake5.lua" echo project "VkBootstrap"
>> "Vendor\vk-bootstrap\premake5.lua" echo     location "."
>> "Vendor\vk-bootstrap\premake5.lua" echo     kind "StaticLib"
>> "Vendor\vk-bootstrap\premake5.lua" echo.
>> "Vendor\vk-bootstrap\premake5.lua" echo     SetupProject()
>> "Vendor\vk-bootstrap\premake5.lua" echo.
>> "Vendor\vk-bootstrap\premake5.lua" echo     files
>> "Vendor\vk-bootstrap\premake5.lua" echo     {
>> "Vendor\vk-bootstrap\premake5.lua" echo         "src/VkBootstrap.h",
>> "Vendor\vk-bootstrap\premake5.lua" echo         "src/VkBootstrap.cpp",
>> "Vendor\vk-bootstrap\premake5.lua" echo         "src/VkBootstrapDispatch.h",
>> "Vendor\vk-bootstrap\premake5.lua" echo         "src/VkBootstrapFeatureChain.h",
>> "Vendor\vk-bootstrap\premake5.lua" echo         "src/VkBootstrapFeatureChain.inl"
>> "Vendor\vk-bootstrap\premake5.lua" echo     }
>> "Vendor\vk-bootstrap\premake5.lua" echo.
>> "Vendor\vk-bootstrap\premake5.lua" echo     includedirs
>> "Vendor\vk-bootstrap\premake5.lua" echo     {
>> "Vendor\vk-bootstrap\premake5.lua" echo         "src"
>> "Vendor\vk-bootstrap\premake5.lua" echo     }
>> "Vendor\vk-bootstrap\premake5.lua" echo.
>> "Vendor\vk-bootstrap\premake5.lua" echo     externalincludedirs
>> "Vendor\vk-bootstrap\premake5.lua" echo     {
>> "Vendor\vk-bootstrap\premake5.lua" echo         IncludeDir["VulkanHeaders"]
>> "Vendor\vk-bootstrap\premake5.lua" echo     }
>> "Vendor\vk-bootstrap\premake5.lua" echo.
>> "Vendor\vk-bootstrap\premake5.lua" echo     ConfigureSanitizers()
>> "Vendor\vk-bootstrap\premake5.lua" echo     ConfigureCommonProject()

if not exist "Vendor\vk-bootstrap\premake5.lua" exit /b 1

exit /b 0

:ensure_imgui_premake
if not exist "Vendor\imgui\" (
    echo [Setup] Vendor\imgui was not found after submodule sync.
    exit /b 1
)

> "Vendor\imgui\premake5.lua" echo project "ImGui"
>> "Vendor\imgui\premake5.lua" echo     location "."
>> "Vendor\imgui\premake5.lua" echo     kind "StaticLib"
>> "Vendor\imgui\premake5.lua" echo.
>> "Vendor\imgui\premake5.lua" echo     SetupProject()
>> "Vendor\imgui\premake5.lua" echo.
>> "Vendor\imgui\premake5.lua" echo     files
>> "Vendor\imgui\premake5.lua" echo     {
>> "Vendor\imgui\premake5.lua" echo         "imgui.h",
>> "Vendor\imgui\premake5.lua" echo         "imgui_internal.h",
>> "Vendor\imgui\premake5.lua" echo         "imconfig.h",
>> "Vendor\imgui\premake5.lua" echo         "imstb_rectpack.h",
>> "Vendor\imgui\premake5.lua" echo         "imstb_textedit.h",
>> "Vendor\imgui\premake5.lua" echo         "imstb_truetype.h",
>> "Vendor\imgui\premake5.lua" echo         "imgui.cpp",
>> "Vendor\imgui\premake5.lua" echo         "imgui_draw.cpp",
>> "Vendor\imgui\premake5.lua" echo         "imgui_tables.cpp",
>> "Vendor\imgui\premake5.lua" echo         "imgui_widgets.cpp",
>> "Vendor\imgui\premake5.lua" echo         "backends/imgui_impl_sdl3.h",
>> "Vendor\imgui\premake5.lua" echo         "backends/imgui_impl_sdl3.cpp",
>> "Vendor\imgui\premake5.lua" echo         "backends/imgui_impl_vulkan.h",
>> "Vendor\imgui\premake5.lua" echo         "backends/imgui_impl_vulkan.cpp"
>> "Vendor\imgui\premake5.lua" echo     }
>> "Vendor\imgui\premake5.lua" echo.
>> "Vendor\imgui\premake5.lua" echo     includedirs
>> "Vendor\imgui\premake5.lua" echo     {
>> "Vendor\imgui\premake5.lua" echo         ".",
>> "Vendor\imgui\premake5.lua" echo         "backends"
>> "Vendor\imgui\premake5.lua" echo     }
>> "Vendor\imgui\premake5.lua" echo.
>> "Vendor\imgui\premake5.lua" echo     externalincludedirs
>> "Vendor\imgui\premake5.lua" echo     {
>> "Vendor\imgui\premake5.lua" echo         IncludeDir["SDL3"],
>> "Vendor\imgui\premake5.lua" echo         IncludeDir["VulkanHeaders"]
>> "Vendor\imgui\premake5.lua" echo     }
>> "Vendor\imgui\premake5.lua" echo.
>> "Vendor\imgui\premake5.lua" echo     ConfigureSanitizers()
>> "Vendor\imgui\premake5.lua" echo     ConfigureCommonProject()

if not exist "Vendor\imgui\premake5.lua" exit /b 1

exit /b 0

:resolve_cmake
where cmake >nul 2>&1
if not errorlevel 1 set "CMAKE_CMD=cmake"
if defined CMAKE_CMD exit /b 0

set "CMAKE_DIR=Scripts\CMake\windows"
set "CMAKE_ROOT=%CMAKE_DIR%\cmake-%CMAKE_VERSION%-windows-x86_64"
set "CMAKE_EXE=%CMAKE_ROOT%\bin\cmake.exe"
if exist "%CMAKE_EXE%" (
    set "CMAKE_CMD=%CMAKE_EXE%"
    exit /b 0
)

set "CMAKE_ARCHIVE=%CMAKE_DIR%\cmake-%CMAKE_VERSION%-windows-x86_64.zip"
set "CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v%CMAKE_VERSION%/cmake-%CMAKE_VERSION%-windows-x86_64.zip"

echo [Setup] CMake was not found. Downloading CMake %CMAKE_VERSION% for Windows...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; New-Item -ItemType Directory -Force -Path '%CMAKE_DIR%' | Out-Null; Invoke-WebRequest -Uri '%CMAKE_URL%' -OutFile '%CMAKE_ARCHIVE%'; Expand-Archive -Path '%CMAKE_ARCHIVE%' -DestinationPath '%CMAKE_DIR%' -Force; Remove-Item '%CMAKE_ARCHIVE%' -Force"
if errorlevel 1 exit /b 1

if exist "%CMAKE_EXE%" (
    set "CMAKE_CMD=%CMAKE_EXE%"
    exit /b 0
)

echo [Setup] CMake download failed.
exit /b 1

:build_sdl
call :resolve_sdl_generator
if errorlevel 1 exit /b 1

call :build_sdl_config Debug
if errorlevel 1 exit /b 1

call :build_sdl_config Release
if errorlevel 1 exit /b 1

exit /b 0

:resolve_sdl_generator
set "SDL_CMAKE_GENERATOR="
if /i "%PREMAKE_ACTION%"=="vs2022" set "SDL_CMAKE_GENERATOR=Visual Studio 17 2022"
if /i "%PREMAKE_ACTION%"=="vs2019" set "SDL_CMAKE_GENERATOR=Visual Studio 16 2019"
if /i "%PREMAKE_ACTION%"=="vs2017" set "SDL_CMAKE_GENERATOR=Visual Studio 15 2017"
if defined SDL_CMAKE_GENERATOR exit /b 0

echo [Setup] Unsupported Windows Premake action for SDL build: %PREMAKE_ACTION%
exit /b 1

:build_sdl_config
set "SDL_CONFIG=%~1"
set "SDL_BUILD_DIR=Vendor\SDL3\Build\windows\%TARGET_ARCH%\%SDL_CONFIG%"
set "SDL_INSTALL_DIR=%CD%\Vendor\SDL3\Install\windows\%TARGET_ARCH%\%SDL_CONFIG%"

if exist "%SDL_BUILD_DIR%\CMakeCache.txt" if exist "%SDL_INSTALL_DIR%\lib\SDL3.lib" if exist "%SDL_INSTALL_DIR%\bin\SDL3.dll" (
    echo [Setup] SDL3 %SDL_CONFIG% is already available. Skipping build.
    exit /b 0
)

if /i "%TARGET_ARCH%"=="arm64" (
    set "SDL_CMAKE_ARCHITECTURE=ARM64"
) else (
    set "SDL_CMAKE_ARCHITECTURE=x64"
)

echo [Setup] Building SDL3 (%SDL_CONFIG%)...
"%CMAKE_CMD%" -S "Vendor\SDL3" -B "%SDL_BUILD_DIR%" -G "%SDL_CMAKE_GENERATOR%" -A %SDL_CMAKE_ARCHITECTURE% -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=ON -DCMAKE_INSTALL_PREFIX="%SDL_INSTALL_DIR%"
if errorlevel 1 exit /b 1

"%CMAKE_CMD%" --build "%SDL_BUILD_DIR%" --config %SDL_CONFIG% --target install
if errorlevel 1 exit /b 1

exit /b 0

:resolve_premake
set "PREMAKE_ACTION=%~1"
if "%PREMAKE_ACTION%"=="" set "PREMAKE_ACTION=vs2022"

set "PREMAKE_CMD="
set "PREMAKE_DIR=Scripts\Premake\windows"
set "PREMAKE_EXE=%PREMAKE_DIR%\premake5.exe"
if exist "%PREMAKE_EXE%" (
    set "PREMAKE_CMD=%PREMAKE_EXE%"
    exit /b 0
)

where premake5 >nul 2>&1
if not errorlevel 1 set "PREMAKE_CMD=premake5"
if defined PREMAKE_CMD exit /b 0

where premake5.exe >nul 2>&1
if not errorlevel 1 set "PREMAKE_CMD=premake5.exe"
if defined PREMAKE_CMD exit /b 0

set "PREMAKE_ARCHIVE=%PREMAKE_DIR%\premake-%PREMAKE_VERSION%-windows.zip"
set "PREMAKE_URL=https://github.com/premake/premake-core/releases/download/v%PREMAKE_VERSION%/premake-%PREMAKE_VERSION%-windows.zip"

echo [Setup] Premake was not found. Downloading Premake %PREMAKE_VERSION% for Windows...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; New-Item -ItemType Directory -Force -Path '%PREMAKE_DIR%' | Out-Null; Invoke-WebRequest -Uri '%PREMAKE_URL%' -OutFile '%PREMAKE_ARCHIVE%'; Expand-Archive -Path '%PREMAKE_ARCHIVE%' -DestinationPath '%PREMAKE_DIR%' -Force; Remove-Item '%PREMAKE_ARCHIVE%' -Force"
if errorlevel 1 exit /b 1

if exist "%PREMAKE_EXE%" (
    set "PREMAKE_CMD=%PREMAKE_EXE%"
    exit /b 0
)

echo [Setup] Premake download failed.
exit /b 1

:resolve_vulkan_sdk
set "LIFE_VULKAN_SDK="

rem Check VULKAN_SDK environment variable first
if defined VULKAN_SDK (
    if exist "%VULKAN_SDK%\Lib\vulkan-1.lib" (
        set "LIFE_VULKAN_SDK=%VULKAN_SDK%"
        echo [Setup] Vulkan SDK found via VULKAN_SDK at %VULKAN_SDK%
        exit /b 0
    )
)

rem Check default install location
if exist "C:\VulkanSDK\%VULKAN_SDK_VERSION%\Lib\vulkan-1.lib" (
    set "LIFE_VULKAN_SDK=C:\VulkanSDK\%VULKAN_SDK_VERSION%"
    echo [Setup] Vulkan SDK found at C:\VulkanSDK\%VULKAN_SDK_VERSION%
    exit /b 0
)

rem Check local vendor copy
set "VULKAN_LOCAL_DIR=Vendor\VulkanSDK\%VULKAN_SDK_VERSION%"
if exist "%VULKAN_LOCAL_DIR%\Lib\vulkan-1.lib" (
    set "LIFE_VULKAN_SDK=%CD%\%VULKAN_LOCAL_DIR%"
    echo [Setup] Vulkan SDK found at %VULKAN_LOCAL_DIR%
    exit /b 0
)

rem Download and install locally
set "VULKAN_INSTALLER=Vendor\VulkanSDK\vulkan_sdk_%VULKAN_SDK_VERSION%.exe"
set "VULKAN_URL=https://sdk.lunarg.com/sdk/download/%VULKAN_SDK_VERSION%/windows/vulkan_sdk.exe"

if not exist "Vendor\VulkanSDK" mkdir "Vendor\VulkanSDK"

echo [Setup] Vulkan SDK not found. Downloading Vulkan SDK %VULKAN_SDK_VERSION%...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; Invoke-WebRequest -Uri '%VULKAN_URL%' -OutFile '%VULKAN_INSTALLER%'"
if errorlevel 1 (
    echo [Setup] Failed to download Vulkan SDK. Please install manually from https://vulkan.lunarg.com/sdk/home
    exit /b 1
)

echo [Setup] Installing Vulkan SDK %VULKAN_SDK_VERSION% locally (this may take a few minutes)...
"%VULKAN_INSTALLER%" --root "%CD%\%VULKAN_LOCAL_DIR%" --accept-licenses --default-answer --confirm-command install copy_only=1
if errorlevel 1 (
    echo [Setup] Vulkan SDK installation failed. Please install manually from https://vulkan.lunarg.com/sdk/home
    exit /b 1
)

del "%VULKAN_INSTALLER%" >nul 2>&1

if exist "%VULKAN_LOCAL_DIR%\Lib\vulkan-1.lib" (
    set "LIFE_VULKAN_SDK=%CD%\%VULKAN_LOCAL_DIR%"
    echo [Setup] Vulkan SDK %VULKAN_SDK_VERSION% installed locally.
    exit /b 0
)

echo [Setup] Vulkan SDK installation did not produce expected files. Please install manually.
exit /b 1

:build_nvrhi
call :build_nvrhi_config Debug
if errorlevel 1 exit /b 1

call :build_nvrhi_config Release
if errorlevel 1 exit /b 1

exit /b 0

:nvrhi_install_ready
set "NVRHI_READY=0"
if not defined NVRHI_INSTALL_DIR exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\include\nvrhi\nvrhi.h" exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\lib\nvrhi.lib" exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\lib\nvrhi_vk.lib" exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\lib\nvrhi_d3d12.lib" exit /b 0
set "NVRHI_READY=1"
exit /b 0

:build_nvrhi_config
set "NVRHI_CONFIG=%~1"
set "NVRHI_BUILD_DIR=Vendor\nvrhi\Build\windows\%TARGET_ARCH%\%NVRHI_CONFIG%"
set "NVRHI_INSTALL_DIR=%CD%\Vendor\nvrhi\Install\windows\%TARGET_ARCH%\%NVRHI_CONFIG%"

call :nvrhi_install_ready
if errorlevel 1 exit /b 1
if "%NVRHI_READY%"=="1" (
    echo [Setup] NVRHI %NVRHI_CONFIG% is already available. Skipping build.
    exit /b 0
)

if /i "%TARGET_ARCH%"=="arm64" (
    set "NVRHI_CMAKE_ARCHITECTURE=ARM64"
) else (
    set "NVRHI_CMAKE_ARCHITECTURE=x64"
)

echo [Setup] Building NVRHI (%NVRHI_CONFIG%)...
"%CMAKE_CMD%" -S "Vendor\nvrhi" -B "%NVRHI_BUILD_DIR%" -G "%SDL_CMAKE_GENERATOR%" -A %NVRHI_CMAKE_ARCHITECTURE% -DNVRHI_WITH_VULKAN=ON -DNVRHI_WITH_DX12=ON -DNVRHI_WITH_DX11=OFF -DNVRHI_WITH_NVAPI=OFF -DNVRHI_WITH_RTXMU=OFF -DNVRHI_WITH_AFTERMATH=OFF -DNVRHI_BUILD_SHARED=OFF -DNVRHI_INSTALL=ON -DCMAKE_INSTALL_PREFIX="%NVRHI_INSTALL_DIR%" "-DCMAKE_CXX_FLAGS=/DNVRHI_SHARED_LIBRARY_BUILD"
if errorlevel 1 exit /b 1

"%CMAKE_CMD%" --build "%NVRHI_BUILD_DIR%" --config %NVRHI_CONFIG% --target install
if errorlevel 1 exit /b 1

exit /b 0

:needs_bootstrap
set "NEEDS_BOOTSTRAP=0"
if not exist ".gitmodules" (
    set "NEEDS_BOOTSTRAP=1"
    exit /b 0
)

call :evaluate_declared_submodules
if errorlevel 1 exit /b 1
exit /b 0

:evaluate_declared_submodules
set "FOUND_DECLARED_SUBMODULE=0"
for /f "usebackq tokens=1,*" %%A in (`git config --file .gitmodules --get-regexp "^submodule\..*\.path$" 2^>nul`) do (
    set "FOUND_DECLARED_SUBMODULE=1"
    set "submodulePath=%%~B"
    git submodule status -- "!submodulePath!" >nul 2>&1
    if errorlevel 1 set "NEEDS_BOOTSTRAP=1"
    if "!NEEDS_BOOTSTRAP!"=="0" if not exist "!submodulePath!\*" set "NEEDS_BOOTSTRAP=1"
)

if "!FOUND_DECLARED_SUBMODULE!"=="0" set "NEEDS_BOOTSTRAP=1"
exit /b 0

:capture_arch_arg
if not defined CURRENT_ARG exit /b 0
if /i "%CURRENT_ARG:~0,7%"=="--arch=" set "TARGET_ARCH=%CURRENT_ARG:~7%"
exit /b 0

:error
echo [Setup] Failed to prepare dependencies.
popd >nul 2>nul
exit /b 1
