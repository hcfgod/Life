param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Configuration = 'Debug',

    [Parameter(Mandatory = $false)]
    [string]$SolutionPath = 'Life.sln',

    [Parameter(Mandatory = $false)]
    [string]$Platform = '',

    [Parameter(Mandatory = $false)]
    [string]$LogPath = ''
)

$ErrorActionPreference = 'Stop'

function Resolve-NormalizedPlatform([string]$RequestedPlatform) {
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
        'arm64' { return 'ARM64' }
        'aarch64' { return 'ARM64' }
        default { throw "Unsupported MSBuild platform '$RequestedPlatform'." }
    }
}

function Find-MSBuildPath() {
    $msbuildCommand = Get-Command 'msbuild.exe' -ErrorAction SilentlyContinue
    if ($null -ne $msbuildCommand) {
        return $msbuildCommand.Source
    }

    $vsWherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vsWherePath) {
        $msbuildPath = & $vsWherePath -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($msbuildPath) -and (Test-Path -LiteralPath $msbuildPath)) {
            return $msbuildPath
        }
    }

    throw "Unable to locate MSBuild.exe. Install Visual Studio Build Tools with MSBuild, or run from a Developer PowerShell environment."
}

$Platform = Resolve-NormalizedPlatform $Platform
$MSBuildPath = Find-MSBuildPath

if ($LogPath -ne '') {
    $logDirectory = Split-Path -Path $LogPath -Parent
    if ($logDirectory -ne '' -and -not (Test-Path -LiteralPath $logDirectory)) {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }
}

$arguments = @(
    $SolutionPath,
    '/m',
    '/restore',
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    '/verbosity:minimal'
)

if ($LogPath -ne '') {
    $arguments += '/fl'
    $arguments += "/flp:logfile=$LogPath;verbosity=diagnostic"
}

& $MSBuildPath @arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
