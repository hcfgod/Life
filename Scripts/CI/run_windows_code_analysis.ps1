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
    [string]$LogPath = ''
)

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
    '/p:RunCodeAnalysis=true',
    '/p:EnableCppCoreCheck=true',
    '/p:CodeAnalysisTreatWarningsAsErrors=true',
    '/verbosity:minimal'
)

if ($RulesetPath -ne '') {
    $resolvedRulesetPath = (Resolve-Path -LiteralPath $RulesetPath).Path
    $arguments += "/p:CodeAnalysisRuleSet=$resolvedRulesetPath"
}

if ($LogPath -ne '') {
    $arguments += '/fl'
    $arguments += "/flp:logfile=$LogPath;verbosity=diagnostic"
}

& msbuild.exe @arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
