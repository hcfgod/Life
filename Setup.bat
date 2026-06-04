@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PREMAKE_VERSION=5.0.0-beta2"
set "CMAKE_VERSION=4.3.0"
set "VULKAN_SDK_VERSION=1.4.304.1"
set "CMAKE_CMD="
set "TARGET_ARCH="
set "SDL_CMAKE_ARCHITECTURE="
set "SETUP_INTERACTIVE=0"
if "%~1"=="" set "SETUP_INTERACTIVE=1"
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

call :ensure_entt_vendor
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

if "%SETUP_INTERACTIVE%"=="1" (
    echo [Setup] Dependencies, SDL3, and NVRHI are ready.
    call :interactive_menu
    if errorlevel 1 goto :error
) else (
    call :generate_project_files "%PREMAKE_ACTION%"
    if errorlevel 1 goto :error
    echo [Setup] Dependencies, SDL3, NVRHI, and project files are ready.
)

popd >nul
exit /b 0

:generate_project_files
set "GENERATE_ACTION=%~1"
if "%GENERATE_ACTION%"=="" set "GENERATE_ACTION=vs2022"
echo [Setup] Resolved Premake command: "%PREMAKE_CMD%"
echo [Setup] Generating project files with Premake (%GENERATE_ACTION%)...
call "%PREMAKE_CMD%" %GENERATE_ACTION% %PREMAKE_EXTRA_ARGS%
exit /b %ERRORLEVEL%

:interactive_menu
echo.
echo [Setup] What would you like to do next?
echo   1. Generate Project Files
echo   2. Build
echo   3. Clean
echo   4. Exit
set "SETUP_MENU_CHOICE="
set /p SETUP_MENU_CHOICE="Choose an option [1-4]: "
if "%SETUP_MENU_CHOICE%"=="1" (
    call :prompt_generate_project_files
    if errorlevel 1 exit /b 1
    goto :interactive_menu
)
if "%SETUP_MENU_CHOICE%"=="2" (
    call :prompt_build
    if errorlevel 1 exit /b 1
    goto :interactive_menu
)
if "%SETUP_MENU_CHOICE%"=="3" (
    call :clean_generated_outputs
    if errorlevel 1 exit /b 1
    goto :interactive_menu
)
if "%SETUP_MENU_CHOICE%"=="4" exit /b 0
echo [Setup] Invalid option.
goto :interactive_menu

:prompt_generate_project_files
echo.
echo [Setup] Select a Premake action:
echo   1. vs2022
echo   2. vs2019
echo   3. vs2017
set "PREMAKE_MENU_CHOICE="
set /p PREMAKE_MENU_CHOICE="Choose an option [1-3]: "
if "%PREMAKE_MENU_CHOICE%"=="1" (
    call :generate_project_files "vs2022"
    exit /b !ERRORLEVEL!
)
if "%PREMAKE_MENU_CHOICE%"=="2" (
    call :generate_project_files "vs2019"
    exit /b !ERRORLEVEL!
)
if "%PREMAKE_MENU_CHOICE%"=="3" (
    call :generate_project_files "vs2017"
    exit /b !ERRORLEVEL!
)
echo [Setup] Invalid Premake action.
exit /b 1

:prompt_build
echo.
echo [Setup] Select a build configuration:
echo   1. Debug
echo   2. Release
echo   3. Dist
set "BUILD_CONFIGURATION="
set "BUILD_MENU_CHOICE="
set /p BUILD_MENU_CHOICE="Choose an option [1-3]: "
if "%BUILD_MENU_CHOICE%"=="1" set "BUILD_CONFIGURATION=Debug"
if "%BUILD_MENU_CHOICE%"=="2" set "BUILD_CONFIGURATION=Release"
if "%BUILD_MENU_CHOICE%"=="3" set "BUILD_CONFIGURATION=Dist"
if not defined BUILD_CONFIGURATION (
    echo [Setup] Invalid build configuration.
    exit /b 1
)
echo [Setup] Generating vs2022 project files before build...
call :generate_project_files "vs2022"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "Scripts\CI\build_windows.ps1" -Configuration %BUILD_CONFIGURATION%
set "BUILD_CONFIGURATION="
exit /b %ERRORLEVEL%

:clean_generated_outputs
echo [Setup] Cleaning generated project and build outputs...
if exist ".vs" rmdir /s /q ".vs"
if exist "Build" rmdir /s /q "Build"
if exist "bin-int" rmdir /s /q "bin-int"
if exist "Life.sln" del /q "Life.sln"
if exist "Makefile" del /q "Makefile"
for %%D in (Editor Engine Runtime Test) do (
    for %%F in (%%D\*.vcxproj %%D\*.vcxproj.filters %%D\*.vcxproj.user %%D\*.vcxitems %%D\*.vcxitems.filters) do (
        if exist "%%F" (
            echo [Setup] Removing %%F
            del /q "%%F"
        )
    )
    for /d %%F in (%%D\*.xcodeproj) do (
        echo [Setup] Removing %%F
        rmdir /s /q "%%F"
    )
)
exit /b 0

:ensure_entt_vendor
if not exist "Vendor\entt\src\entt\entt.hpp" (
    echo [Setup] Vendor\entt was not found after submodule sync.
    exit /b 1
)

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

:nvrhi_install_stamp_ready
set "NVRHI_STAMP_READY=0"
if not defined NVRHI_INSTALL_DIR exit /b 0
set "NVRHI_INSTALL_STAMP=%NVRHI_INSTALL_DIR%\life_nvrhi_dispatch_owner.txt"
if not exist "%NVRHI_INSTALL_STAMP%" exit /b 0
findstr /x /c:"engine_dispatch_owner_v5" "%NVRHI_INSTALL_STAMP%" >nul 2>&1
if errorlevel 1 exit /b 0
set "NVRHI_STAMP_READY=1"
exit /b 0

:nvrhi_install_ready
set "NVRHI_READY=0"
if not defined NVRHI_INSTALL_DIR exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\include\nvrhi\nvrhi.h" exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\lib\nvrhi.lib" exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\lib\nvrhi_vk.lib" exit /b 0
if not exist "%NVRHI_INSTALL_DIR%\lib\nvrhi_d3d12.lib" exit /b 0
call :nvrhi_install_stamp_ready
if errorlevel 1 exit /b 1
if not "%NVRHI_STAMP_READY%"=="1" exit /b 0
set "NVRHI_READY=1"
exit /b 0

:build_nvrhi_config
set "NVRHI_CONFIG=%~1"
set "NVRHI_BUILD_DIR=Vendor\nvrhi\Build\windows\%TARGET_ARCH%\%NVRHI_CONFIG%"
set "NVRHI_INSTALL_DIR=%CD%\Vendor\nvrhi\Install\windows\%TARGET_ARCH%\%NVRHI_CONFIG%"
set "NVRHI_INSTALL_STAMP=%NVRHI_INSTALL_DIR%\life_nvrhi_dispatch_owner.txt"

call :nvrhi_install_ready
if errorlevel 1 exit /b 1
if "%NVRHI_READY%"=="1" (
    echo [Setup] NVRHI %NVRHI_CONFIG% is already available. Skipping build.
    exit /b 0
)

if exist "%NVRHI_INSTALL_DIR%\lib\nvrhi_vk.lib" (
    echo [Setup] Rebuilding NVRHI %NVRHI_CONFIG% to refresh Vulkan dispatcher ownership.
    if exist "%NVRHI_BUILD_DIR%" rmdir /s /q "%NVRHI_BUILD_DIR%"
    if exist "%NVRHI_INSTALL_DIR%" rmdir /s /q "%NVRHI_INSTALL_DIR%"
)

if /i "%TARGET_ARCH%"=="arm64" (
    set "NVRHI_CMAKE_ARCHITECTURE=ARM64"
) else (
    set "NVRHI_CMAKE_ARCHITECTURE=x64"
)

echo [Setup] Building NVRHI (%NVRHI_CONFIG%)...
"%CMAKE_CMD%" -S "Vendor\nvrhi" -B "%NVRHI_BUILD_DIR%" -G "%SDL_CMAKE_GENERATOR%" -A %NVRHI_CMAKE_ARCHITECTURE% -DNVRHI_WITH_VULKAN=ON -DNVRHI_WITH_DX12=ON -DNVRHI_WITH_DX11=OFF -DNVRHI_WITH_NVAPI=OFF -DNVRHI_WITH_RTXMU=OFF -DNVRHI_WITH_AFTERMATH=OFF -DNVRHI_BUILD_SHARED=OFF -DNVRHI_INSTALL=ON -DCMAKE_INSTALL_PREFIX="%NVRHI_INSTALL_DIR%"
if errorlevel 1 exit /b 1

"%CMAKE_CMD%" --build "%NVRHI_BUILD_DIR%" --config %NVRHI_CONFIG% --target install
if errorlevel 1 exit /b 1

> "%NVRHI_INSTALL_STAMP%" echo engine_dispatch_owner_v5

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
