param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('Debug', 'Release', 'Dist')]
    [string]$Configuration = 'Debug',

    [Parameter(Mandatory = $false)]
    [string]$SolutionPath = 'Life.sln',

    [Parameter(Mandatory = $false)]
    [string]$Platform = 'x64',

    [Parameter(Mandatory = $false)]
    [string]$RulesetPath = '',

    [Parameter(Mandatory = $false)]
    [string]$LogPath = '',

    [Parameter(Mandatory = $false)]
    [string[]]$ProjectPaths = @()
)

function Resolve-AnalysisProjectPaths([string]$RequestedSolutionPath, [string[]]$RequestedProjectPaths) {
    if ($RequestedProjectPaths.Count -gt 0) {
        return $RequestedProjectPaths | ForEach-Object { (Resolve-Path -LiteralPath $_).Path }
    }

    $solutionExtension = [System.IO.Path]::GetExtension($RequestedSolutionPath)
    if ($solutionExtension -ieq '.vcxproj') {
        return @((Resolve-Path -LiteralPath $RequestedSolutionPath).Path)
    }

    return @(
        (Resolve-Path -LiteralPath 'Engine/Engine.vcxproj').Path,
        (Resolve-Path -LiteralPath 'Runtime/Runtime.vcxproj').Path,
        (Resolve-Path -LiteralPath 'Editor/Editor.vcxproj').Path,
        (Resolve-Path -LiteralPath 'Test/Test.vcxproj').Path
    )
}

if ($LogPath -ne '') {
    $logDirectory = Split-Path -Path $LogPath -Parent
    if ($logDirectory -ne '' -and -not (Test-Path -LiteralPath $logDirectory)) {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }

    if (Test-Path -LiteralPath $LogPath) {
        Remove-Item -LiteralPath $LogPath -Force
    }
}

if ($RulesetPath -ne '') {
    $resolvedRulesetPath = (Resolve-Path -LiteralPath $RulesetPath).Path
}

$analysisProjectPaths = Resolve-AnalysisProjectPaths -RequestedSolutionPath $SolutionPath -RequestedProjectPaths $ProjectPaths

foreach ($analysisProjectPath in $analysisProjectPaths) {
    Write-Host "Running code analysis for $(Split-Path -Path $analysisProjectPath -Leaf)"

    $arguments = @(
        $analysisProjectPath,
        '/m',
        '/restore',
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        '/p:RunCodeAnalysis=true',
        '/p:EnableCppCoreCheck=true',
        '/p:CodeAnalysisTreatWarningsAsErrors=true',
        '/p:BuildProjectReferences=false',
        '/verbosity:minimal'
    )

    if ($RulesetPath -ne '') {
        $arguments += "/p:CodeAnalysisRuleSet=$resolvedRulesetPath"
    }

    if ($LogPath -ne '') {
        $arguments += '/fl'
        $arguments += "/flp:logfile=$LogPath;verbosity=diagnostic;append"
    }

    & msbuild.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
