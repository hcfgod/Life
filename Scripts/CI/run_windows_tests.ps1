param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Configuration = 'Debug'
)

$candidates = @(
    "Build/windows-x64/$Configuration/Test/Test.exe",
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
