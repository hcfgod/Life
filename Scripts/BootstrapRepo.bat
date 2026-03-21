@echo off
setlocal EnableExtensions

pushd "%~dp0\.." >nul || goto :error

echo [Bootstrap] Registering repository dependencies...
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [Bootstrap] Initializing git repository...
    git init
    if errorlevel 1 goto :error
)

call :ensure_submodule "Vendor/SDL3" "https://github.com/libsdl-org/SDL.git"
if errorlevel 1 goto :error
call :ensure_submodule "Vendor/spdlog" "https://github.com/gabime/spdlog.git"
if errorlevel 1 goto :error
call :ensure_submodule "Vendor/json" "https://github.com/nlohmann/json.git"
if errorlevel 1 goto :error
call :ensure_submodule "Vendor/doctest" "https://github.com/doctest/doctest.git"
if errorlevel 1 goto :error

echo [Bootstrap] Syncing submodules...
git submodule update --init --recursive
if errorlevel 1 goto :error

echo [Bootstrap] Repository dependencies are ready.
popd >nul
exit /b 0

:ensure_submodule
set "submodulePath=%~1"
set "submoduleUrl=%~2"

if exist .gitmodules (
    findstr /i /c:"path = %submodulePath%" .gitmodules >nul
    if not errorlevel 1 (
        git submodule status -- "%submodulePath%" >nul 2>&1
        if not errorlevel 1 (
            echo [Bootstrap] %submodulePath% already registered.
            exit /b 0
        )
    )
)

if exist "%submodulePath%" (
    dir /b "%submodulePath%" 2>nul | findstr . >nul
    if errorlevel 1 (
        rmdir "%submodulePath%"
    ) else (
        echo [Bootstrap] %submodulePath% already exists and will be reused.
        exit /b 0
    )
)

echo [Bootstrap] Adding %submodulePath%...
git submodule add --force "%submoduleUrl%" "%submodulePath%"
if errorlevel 1 exit /b 1

exit /b 0

:error
echo [Bootstrap] Failed to bootstrap repository dependencies.
popd >nul 2>nul
exit /b 1
