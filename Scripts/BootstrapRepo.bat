@echo off
setlocal EnableExtensions EnableDelayedExpansion

pushd "%~dp0\.." >nul || goto :error

echo [Bootstrap] Registering repository dependencies...
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [Bootstrap] Initializing git repository...
    git init
    if errorlevel 1 goto :error
)

call :register_declared_submodules
if errorlevel 1 goto :error

echo [Bootstrap] Syncing submodules...
git submodule update --init --recursive
if errorlevel 1 goto :error

echo [Bootstrap] Repository dependencies are ready.
popd >nul
exit /b 0

:register_declared_submodules
if not exist ".gitmodules" (
    echo [Bootstrap] .gitmodules was not found. No dependencies to register.
    exit /b 0
)

set "SUBMODULES_FOUND=0"
for /f "usebackq tokens=1,*" %%A in (`git config --file .gitmodules --get-regexp "^submodule\..*\.path$" 2^>nul`) do (
    set "SUBMODULES_FOUND=1"
    set "submoduleKey=%%~A"
    set "submodulePath=%%~B"
    set "submoduleUrl="
    set "submoduleBranch="
    call set "submoduleUrlKey=%%submoduleKey:.path=.url%%"
    call set "submoduleBranchKey=%%submoduleKey:.path=.branch%%"
    for /f "usebackq delims=" %%U in (`git config --file .gitmodules --get "!submoduleUrlKey!" 2^>nul`) do set "submoduleUrl=%%~U"
    for /f "usebackq delims=" %%B in (`git config --file .gitmodules --get "!submoduleBranchKey!" 2^>nul`) do set "submoduleBranch=%%~B"
    if not defined submoduleUrl (
        echo [Bootstrap] Skipping !submodulePath! because no submodule URL is declared.
    ) else (
        call :ensure_submodule "!submodulePath!" "!submoduleUrl!" "!submoduleBranch!"
        if errorlevel 1 exit /b 1
    )
)

if "!SUBMODULES_FOUND!"=="0" (
    echo [Bootstrap] No submodules were declared in .gitmodules.
)

exit /b 0

:ensure_submodule
set "submodulePath=%~1"
set "submoduleUrl=%~2"
set "submoduleBranch=%~3"

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

set "submoduleAddArgs="
if defined submoduleBranch set "submoduleAddArgs=-b %submoduleBranch%"

echo [Bootstrap] Adding %submodulePath%...
git submodule add --force %submoduleAddArgs% "%submoduleUrl%" "%submodulePath%"
if errorlevel 1 exit /b 1

exit /b 0

:error
echo [Bootstrap] Failed to bootstrap repository dependencies.
popd >nul 2>nul
exit /b 1
