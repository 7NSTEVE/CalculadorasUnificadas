[CmdletBinding()]
param(
    [string]$LogPath = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "f7w71_ref.cpp"
$outDir = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$outExe = Join-Path $outDir "f7w71_ref.exe"

$candidates = New-Object System.Collections.Generic.List[string]
if ($env:VSINSTALLDIR) {
    $candidates.Add((Join-Path $env:VSINSTALLDIR "VC\Auxiliary\Build\vcvars32.bat"))
}
$candidates.Add("D:\DevTools\VSBuildTools\VC\Auxiliary\Build\vcvars32.bat")
$candidates.Add("C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat")
$candidates.Add("C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat")
$candidates.Add("C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat")
$candidates.Add("C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat")

$vcvars = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vcvars) {
    throw "vcvars32.bat not found; install MSVC x86 build tools or edit this script."
}

$cl = 'cl /nologo /utf-8 /EHsc /std:c++17 /O2 /W4 /Fe:"{0}" "{1}" /link version.lib' -f $outExe, $src
$command = 'call "{0}" >nul && {1}' -f $vcvars, $cl

$output = & cmd.exe /d /c $command 2>&1
$exitCode = $LASTEXITCODE
$output | ForEach-Object { Write-Output $_ }

if ($LogPath) {
    $logDir = Split-Path -Parent $LogPath
    if ($logDir) { New-Item -ItemType Directory -Force -Path $logDir | Out-Null }
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("F7-W 7.1 S1 harness build")
    $lines.Add("generated_utc: " + (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ"))
    $lines.Add("vcvars: $vcvars")
    $lines.Add("command: $command")
    $lines.Add("exit_code: $exitCode")
    $lines.Add("output:")
    foreach ($line in $output) { $lines.Add([string]$line) }
    Set-Content -LiteralPath $LogPath -Value $lines -Encoding UTF8
}

exit $exitCode
