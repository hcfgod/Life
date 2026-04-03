param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Configuration = 'Debug',

    [Parameter(Mandatory = $false)]
    [string]$Platform = ''
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

$platformSuffix = Resolve-PlatformSuffix $Platform
$testBinary = Find-TestBinary -PlatformSuffix $platformSuffix -BuildConfiguration $Configuration
$testDirectory = Split-Path -Parent $testBinary
$sdlBinDirectory = Find-SdlBinDirectory -PlatformSuffix $platformSuffix -BuildConfiguration $Configuration

$pathEntries = @($testDirectory)
if ($null -ne $sdlBinDirectory) {
    $pathEntries += $sdlBinDirectory
}

$env:PATH = (($pathEntries | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) + $env:PATH) -join ';'

Write-Host "[CI] Repo root: $RepoRoot"
Write-Host "[CI] Test binary: $testBinary"
Write-Host "[CI] Working directory: $testDirectory"
if ($null -ne $sdlBinDirectory) {
    Write-Host "[CI] SDL runtime directory: $sdlBinDirectory"
}

Push-Location $testDirectory
try {
    & $testBinary
    $testExitCode = $LASTEXITCODE
}
finally {
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
