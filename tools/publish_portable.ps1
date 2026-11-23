param(
    [Parameter(Mandatory=$true)][string]$TargetDir,
    [string]$MonoRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'
# Local staging helper only; never runs git push or modifies remote visibility.
New-Item -ItemType Directory -Force $TargetDir | Out-Null
Write-Host "Local portable staging helper ready. Target: $TargetDir (no git push performed)."
