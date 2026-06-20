param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Configuration = 'Debug',

    [Parameter(Mandatory = $false)]
    [string]$Platform = '',

    [Parameter(Mandatory = $false)]
    [switch]$LiveBackendSmoke
)

$ErrorActionPreference = 'Stop'
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $ScriptRoot '..\..')).Path

function Resolve-PlatformSuffix([string]$RequestedPlatform) {
    if (-not [string]::IsNullOrWhiteSpace($RequestedPlatform)) {
        $normalized = $RequestedPlatform.Trim().ToLowerInvariant()
    }
    elseif (-not [string]::IsNullOrWhiteSpace($env:PROCESSOR_ARCHITEW6432)) {
        $normalized = $env:PROCESSOR_ARCHITEW6432.Trim().ToLowerInvariant()
    }
    else {
        $normalized = $env:PROCESSOR_ARCHITECTURE.Trim().ToLowerInvariant()
    }

    switch ($normalized) {
        'amd64' { return 'x64' }
        'x86_64' { return 'x64' }
        'x64' { return 'x64' }
        'arm64' { return 'arm64' }
        'aarch64' { return 'arm64' }
        default { throw "Unsupported Windows test platform '$RequestedPlatform'." }
    }
}

function Find-TestBinary([string]$PlatformSuffix, [string]$BuildConfiguration) {
    $candidates = @(
        (Join-Path $RepoRoot "Build/windows-$PlatformSuffix/$BuildConfiguration/Test/Test.exe"),
        (Join-Path $RepoRoot "Build/windows-x64/$BuildConfiguration/Test/Test.exe"),
        (Join-Path $RepoRoot "Build/windows-arm64/$BuildConfiguration/Test/Test.exe"),
        (Join-Path $RepoRoot "Build/windows-x86_64/$BuildConfiguration/Test/Test.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $availableBinaries = @(Get-ChildItem -Path (Join-Path $RepoRoot 'Build') -Filter Test.exe -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
    $searchedPaths = $candidates -join [Environment]::NewLine
    $availablePaths = if ($availableBinaries.Count -gt 0) { $availableBinaries -join [Environment]::NewLine } else { '<none>' }
    throw "Unable to find Windows Test binary for configuration '$BuildConfiguration'. Searched:`n$searchedPaths`nAvailable Test.exe files:`n$availablePaths"
}

function Find-SdlBinDirectory([string]$PlatformSuffix, [string]$BuildConfiguration) {
    $candidates = @(
        (Join-Path $RepoRoot "Vendor/SDL3/Install/windows/$PlatformSuffix/$BuildConfiguration/bin"),
        (Join-Path $RepoRoot "Vendor/SDL3/Install/windows/$PlatformSuffix/Release/bin"),
        (Join-Path $RepoRoot "Vendor/SDL3/Install/windows/x64/$BuildConfiguration/bin"),
        (Join-Path $RepoRoot "Vendor/SDL3/Install/windows/x64/Release/bin"),
        (Join-Path $RepoRoot "Vendor/SDL3/Install/windows/arm64/$BuildConfiguration/bin"),
        (Join-Path $RepoRoot "Vendor/SDL3/Install/windows/arm64/Release/bin")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Install-VulkanRuntimeIfNeeded() {
    $vulkanDll = Get-Command 'vulkan-1.dll' -ErrorAction SilentlyContinue
    if ($null -ne $vulkanDll) {
        Write-Host "[CI] Vulkan Runtime already available: $($vulkanDll.Source)"
        return
    }

    if (Test-Path -LiteralPath "$env:SystemRoot\System32\vulkan-1.dll") {
        Write-Host "[CI] Vulkan Runtime found in System32."
        return
    }

    $searchRoots = @()
    if (-not [string]::IsNullOrWhiteSpace($env:VULKAN_SDK)) {
        $searchRoots += $env:VULKAN_SDK
    }
    $vendorRoot = Join-Path $RepoRoot 'Vendor/VulkanSDK'
    if (Test-Path -LiteralPath $vendorRoot) {
        $searchRoots += @(Get-ChildItem -LiteralPath $vendorRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object { $_.FullName })
    }

    foreach ($sdkRoot in $searchRoots) {
        $rtInstaller = Join-Path $sdkRoot 'Helpers/VulkanRT.exe'
        if (Test-Path -LiteralPath $rtInstaller) {
            Write-Host "[CI] Installing Vulkan Runtime from $rtInstaller ..."
            $proc = Start-Process -FilePath $rtInstaller -ArgumentList '/S' -Wait -PassThru
            if ($proc.ExitCode -eq 0) {
                Write-Host "[CI] Vulkan Runtime installed successfully."
            } else {
                Write-Host "[CI] WARNING: Vulkan Runtime installer exited with code $($proc.ExitCode)."
            }
            return
        }
    }

    Write-Host "[CI] WARNING: vulkan-1.dll not found and no VulkanRT installer available."
}

function Find-VulkanBinDirectory() {
    $candidates = @()

    if (-not [string]::IsNullOrWhiteSpace($env:VULKAN_SDK)) {
        $candidates += (Join-Path $env:VULKAN_SDK 'Bin')
    }

    $vendorRoot = Join-Path $RepoRoot 'Vendor/VulkanSDK'
    if (Test-Path -LiteralPath $vendorRoot) {
        $candidates += @(Get-ChildItem -LiteralPath $vendorRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object {
            Join-Path $_.FullName 'Bin'
        })
    }

    $defaultSdkRoot = 'C:\VulkanSDK'
    if (Test-Path -LiteralPath $defaultSdkRoot) {
        $candidates += @(Get-ChildItem -LiteralPath $defaultSdkRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object {
            Join-Path $_.FullName 'Bin'
        })
    }

    foreach ($candidate in ($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        if ((Test-Path -LiteralPath (Join-Path $candidate 'vulkan-1.dll')) -or
            (Test-Path -LiteralPath (Join-Path $candidate 'dxcompiler.dll')) -or
            (Test-Path -LiteralPath (Join-Path $candidate 'shaderc_shared.dll')) -or
            (Test-Path -LiteralPath (Join-Path $candidate 'VkLayer_khronos_validation.json'))) {
            return $candidate
        }
    }

    return $null
}

function Find-VulkanInfoTool([string]$VulkanBinDir) {
    $candidates = @()

    if (-not [string]::IsNullOrWhiteSpace($VulkanBinDir)) {
        $candidates += (Join-Path $VulkanBinDir 'vulkaninfoSDK.exe')
        $candidates += (Join-Path $VulkanBinDir 'vulkaninfo.exe')
    }

    $pathVulkanInfoSdk = Get-Command 'vulkaninfoSDK.exe' -ErrorAction SilentlyContinue
    if ($null -ne $pathVulkanInfoSdk) {
        $candidates += $pathVulkanInfoSdk.Source
    }

    $pathVulkanInfo = Get-Command 'vulkaninfo.exe' -ErrorAction SilentlyContinue
    if ($null -ne $pathVulkanInfo) {
        $candidates += $pathVulkanInfo.Source
    }

    foreach ($candidate in ($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Test-LiveBackendSmokePrerequisites([string]$VulkanBinDir) {
    $vulkanInfoTool = Find-VulkanInfoTool -VulkanBinDir $VulkanBinDir
    if ($null -eq $vulkanInfoTool) {
        Write-Host "[CI] WARNING: Live backend smoke skipped because vulkaninfo is unavailable."
        return $false
    }

    $previousLayerPath = $env:VK_LAYER_PATH
    try {
        if (-not [string]::IsNullOrWhiteSpace($VulkanBinDir)) {
            $env:VK_LAYER_PATH = $VulkanBinDir
        }

        Write-Host "[CI] Checking Vulkan live backend prerequisites with $vulkanInfoTool ..."
        $previousErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $vulkanInfoOutput = & $vulkanInfoTool --summary 2>&1 | Out-String
            $vulkanInfoExitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        $hasSurfaceExtension = $vulkanInfoOutput -match 'VK_KHR_surface'
        $hasWin32SurfaceExtension = $vulkanInfoOutput -match 'VK_KHR_win32_surface'
        $hasPhysicalDevice = $vulkanInfoOutput -match '(?m)^\s*GPU\d+:' -or $vulkanInfoOutput -match '(?m)^\s*deviceName\s*='

        if (-not $hasSurfaceExtension -or -not $hasWin32SurfaceExtension -or -not $hasPhysicalDevice) {
            Write-Host "[CI] WARNING: Live backend smoke skipped because this Windows runner does not expose a Vulkan WSI-capable device."
            Write-Host "[CI] Vulkan prerequisite summary: exit=$vulkanInfoExitCode VK_KHR_surface=$hasSurfaceExtension VK_KHR_win32_surface=$hasWin32SurfaceExtension physicalDevice=$hasPhysicalDevice"
            return $false
        }

        return $true
    }
    finally {
        if ($null -eq $previousLayerPath) {
            Remove-Item Env:\VK_LAYER_PATH -ErrorAction SilentlyContinue
        } else {
            $env:VK_LAYER_PATH = $previousLayerPath
        }
    }
}

$platformSuffix = Resolve-PlatformSuffix $Platform
$testBinary = Find-TestBinary -PlatformSuffix $platformSuffix -BuildConfiguration $Configuration
$testDirectory = Split-Path -Parent $testBinary
$sdlBinDirectory = Find-SdlBinDirectory -PlatformSuffix $platformSuffix -BuildConfiguration $Configuration
$vulkanBinDirectory = Find-VulkanBinDirectory

Install-VulkanRuntimeIfNeeded

$pathEntries = @($testDirectory)
if ($null -ne $sdlBinDirectory) {
    $pathEntries += $sdlBinDirectory
}
if ($null -ne $vulkanBinDirectory) {
    $pathEntries += $vulkanBinDirectory
    $env:VK_LAYER_PATH = $vulkanBinDirectory
}

$env:PATH = (($pathEntries | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) + $env:PATH) -join ';'

Write-Host "[CI] Repo root: $RepoRoot"
Write-Host "[CI] Test binary: $testBinary"
Write-Host "[CI] Working directory: $testDirectory"
if ($null -ne $sdlBinDirectory) {
    Write-Host "[CI] SDL runtime directory: $sdlBinDirectory"
}
if ($null -ne $vulkanBinDirectory) {
    Write-Host "[CI] Vulkan runtime directory: $vulkanBinDirectory"
}

Push-Location $testDirectory
$previousLiveBackendSmoke = $env:LIFE_ENABLE_LIVE_BACKEND_SMOKE
try {
    $testArguments = @()
    $shouldRunTests = $true

    if ($LiveBackendSmoke) {
        if (Test-LiveBackendSmokePrerequisites -VulkanBinDir $vulkanBinDirectory) {
            $env:LIFE_ENABLE_LIVE_BACKEND_SMOKE = '1'
            $testArguments = @('--test-case=Live backend smoke validates one real host frame when explicitly enabled')
            Write-Host "[CI] Live backend smoke test enabled."
        } else {
            Remove-Item Env:\LIFE_ENABLE_LIVE_BACKEND_SMOKE -ErrorAction SilentlyContinue
            Write-Host "[CI] Live backend smoke prerequisites unavailable; skipping live smoke test invocation."
            $shouldRunTests = $false
        }
    }

    if ($shouldRunTests) {
        & $testBinary @testArguments
        $testExitCode = $LASTEXITCODE
    } else {
        $testExitCode = 0
    }
}
finally {
    if ($null -eq $previousLiveBackendSmoke) {
        Remove-Item Env:\LIFE_ENABLE_LIVE_BACKEND_SMOKE -ErrorAction SilentlyContinue
    } else {
        $env:LIFE_ENABLE_LIVE_BACKEND_SMOKE = $previousLiveBackendSmoke
    }
    Pop-Location
}

if ($testExitCode -ne 0) {
    Write-Host "[CI] Windows tests exited with code $testExitCode"

    $crashDirectory = Join-Path $testDirectory 'Crashes'
    if (Test-Path -LiteralPath $crashDirectory) {
        Write-Host "[CI] Crash artifacts found in $crashDirectory"
        Get-ChildItem -LiteralPath $crashDirectory -File | Sort-Object LastWriteTime | ForEach-Object {
            Write-Host "[CI] Crash artifact: $($_.FullName)"
        }
    }

    exit $testExitCode
}
