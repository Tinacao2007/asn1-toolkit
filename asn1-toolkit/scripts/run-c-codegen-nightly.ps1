[CmdletBinding()]
param(
    [switch]$Run,
    [switch]$Resume,
    [ValidateRange(1,7)][int]$ThroughBatch = 7,
    [ValidateRange(0.01,168)][double]$MaxHours = 10,
    [string]$CodexExe
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$runnerArgs = @('-X', 'utf8', (Join-Path $PSScriptRoot 'nightly_c_codegen.py'), '--through-batch', "$ThroughBatch", '--max-hours', "$MaxHours")
if ($Run) { $runnerArgs += '--run' }
if ($Resume) { $runnerArgs += '--resume' }
if ($CodexExe) { $runnerArgs += @('--codex', $CodexExe) }
Push-Location $projectRoot
try {
    & python @runnerArgs
    $runnerExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $runnerExitCode
