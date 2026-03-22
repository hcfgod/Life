param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Configuration = 'Debug',

    [Parameter(Mandatory = $false)]
    [string]$Platform = ''
)

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

$platformSuffix = Resolve-PlatformSuffix $Platform

$candidates = @(
    "Build/windows-$platformSuffix/$Configuration/Test/Test.exe",
    "Build/windows-x64/$Configuration/Test/Test.exe",
    "Build/windows-arm64/$Configuration/Test/Test.exe",
    "Build/windows-x86_64/$Configuration/Test/Test.exe"
)

$testBinary = $null
foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) {
        $testBinary = $candidate
        break
    }
}

if ($null -eq $testBinary) {
    throw "Unable to find Windows Test binary for configuration '$Configuration'."
}

& $testBinary
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
