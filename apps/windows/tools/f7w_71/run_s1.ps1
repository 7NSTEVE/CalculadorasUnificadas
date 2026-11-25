[CmdletBinding()]
param(
    [string]$EvidenceDir = "",
    [string]$RomLax = "",
    [string]$RomVn = "",
    [string]$Dll = "",
    [string]$Python = "",
    [switch]$SkipBuild,
    [switch]$SumsOnly
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$toolsDir = Split-Path -Parent $scriptDir
$root = Split-Path -Parent $toolsDir
$base = Split-Path -Parent $root
$exe = Join-Path $scriptDir "build\f7w71_ref.exe"
$compareScript = Join-Path $scriptDir "f7w71_compare.py"

if (-not $EvidenceDir) { $EvidenceDir = Join-Path $root "verification\2026-10-19-f7w71" }
if (-not $RomLax) { $RomLax = Join-Path $base "ios_app_extracted\App calculadora cientifica ios\Roms\fx991lax.rom" }
if (-not $RomVn) { $RomVn = Join-Path $base "ios_app_extracted\App calculadora cientifica ios\Roms\fx570vnplus.bin" }
if (-not $Dll) { $Dll = Join-Path $base "Calculadora blanca\SimU8engine.dll" }
if (-not $Python) {
    $candidate = "D:\DevTools\Python311\python.exe"
    if (Test-Path -LiteralPath $candidate) { $Python = $candidate } else { $Python = "python" }
}

New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null

function Get-Sha256Upper([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Update-Sums {
    $lines = New-Object System.Collections.Generic.List[string]
    $evidenceFiles = Get-ChildItem -LiteralPath $EvidenceDir -File -Recurse |
        Where-Object { $_.Name -ne "SHA256SUMS.txt" } |
        Sort-Object FullName
    foreach ($f in $evidenceFiles) {
        $rel = $f.FullName.Substring($EvidenceDir.Length).TrimStart('\', '/')
        $lines.Add(("{0}  {1}" -f (Get-Sha256Upper $f.FullName), $rel))
    }
    $toolFiles = @(
        "f7w71_ref.cpp",
        "build.ps1",
        "f7w71_compare.py",
        "run_s1.ps1",
        "README.md",
        "build\f7w71_ref.exe"
    )
    foreach ($rel in $toolFiles) {
        $p = Join-Path $scriptDir $rel
        if (Test-Path -LiteralPath $p) {
            $relPosix = $rel.Replace('\', '/')
            $lines.Add(("{0}  {1}" -f (Get-Sha256Upper $p), "../../tools/f7w_71/$relPosix"))
        }
    }
    Set-Content -LiteralPath (Join-Path $EvidenceDir "SHA256SUMS.txt") -Value $lines -Encoding ASCII
    Write-Output ("SHA256SUMS.txt: {0} entradas" -f $lines.Count)
}

if ($SumsOnly) {
    Update-Sums
    exit 0
}

foreach ($p in @($RomLax, $RomVn, $Dll)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "missing required input: $p" }
}
if (-not (Test-Path -LiteralPath $exe)) { throw "missing harness executable: $exe (build first)" }

$runLog = New-Object System.Collections.Generic.List[string]
function Add-Log([string]$Line) {
    $runLog.Add($Line)
    Write-Output $Line
}

Add-Log "F7-W 7.1 S1 sequence run log"
Add-Log ("generated_utc: " + (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ"))
Add-Log "harness: $exe"
Add-Log "python: $Python"
Add-Log "dll: $Dll"
Add-Log ("dll_sha256: " + (Get-Sha256Upper $Dll))
Add-Log "rom_lax: $RomLax"
Add-Log ("rom_lax_sha256: " + (Get-Sha256Upper $RomLax))
Add-Log "rom_vn: $RomVn"
Add-Log ("rom_vn_sha256: " + (Get-Sha256Upper $RomVn))
Add-Log ""

$failures = New-Object System.Collections.Generic.List[string]

if (-not $SkipBuild) {
    $buildLog = Join-Path $EvidenceDir "build_log_s1.txt"
    Add-Log "[build]"
    Add-Log "command: powershell -NoProfile -ExecutionPolicy Bypass -File `"$scriptDir\build.ps1`" -LogPath `"$buildLog`""
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $scriptDir "build.ps1") -LogPath $buildLog
    $buildCode = $LASTEXITCODE
    Add-Log "exit_code: $buildCode"
    Add-Log ""
    if ($buildCode -ne 0) { $failures.Add("build") }
}

function Invoke-Harness {
    param([string]$Name, [string[]]$ArgList)
    $cmdLine = '"' + $exe + '" ' + ($ArgList -join ' ')
    Add-Log "[$Name]"
    Add-Log "command: $cmdLine"
    $output = & $exe @ArgList 2>&1
    $code = $LASTEXITCODE
    foreach ($line in $output) { Add-Log ("output: " + [string]$line) }
    Add-Log "exit_code: $code"
    Add-Log ""
    if ($code -ne 0) { $failures.Add($Name) }
    return $code
}

$common = @(
    "--keys", "00,10,30,20,60",
    "--steps", "5000000",
    "--hold", "5000",
    "--settle", "150000",
    "--poll", "256"
)

$runs = @(
    @{ Name = "lax_run1"; Model = "lax"; Rom = $RomLax; Out = "s1_lax_run1.json"; Lcd = "s1_lcd_lax_run1" },
    @{ Name = "lax_run2"; Model = "lax"; Rom = $RomLax; Out = "s1_lax_run2.json"; Lcd = "s1_lcd_lax_run2" },
    @{ Name = "vn_run1"; Model = "vn"; Rom = $RomVn; Out = "s1_vn_run1.json"; Lcd = "s1_lcd_vn_run1" },
    @{ Name = "vn_run2"; Model = "vn"; Rom = $RomVn; Out = "s1_vn_run2.json"; Lcd = "s1_lcd_vn_run2" }
)

foreach ($r in $runs) {
    $outPath = Join-Path $EvidenceDir $r.Out
    $lcdDir = Join-Path $EvidenceDir $r.Lcd
    $argList = @("--firmware", $r.Rom, "--model", $r.Model) + $common +
        @("--out", $outPath, "--lcd-dir", $lcdDir)
    Invoke-Harness -Name $r.Name -ArgList $argList | Out-Null
}

function Invoke-Compare {
    param([string]$Name, [string]$A, [string]$B, [string]$Out)
    Add-Log "[$Name]"
    Add-Log "command: `"$Python`" `"$compareScript`" --a `"$A`" --b `"$B`" --out `"$Out`" --label $Name"
    $output = & $Python $compareScript --a $A --b $B --out $Out --label $Name 2>&1
    $code = $LASTEXITCODE
    foreach ($line in $output) { Add-Log ("output: " + [string]$line) }
    Add-Log "exit_code: $code"
    Add-Log ""
    if ($code -ne 0) { $failures.Add($Name) }
}

Invoke-Compare -Name "lax_determinism" `
    -A (Join-Path $EvidenceDir "s1_lax_run1.json") `
    -B (Join-Path $EvidenceDir "s1_lax_run2.json") `
    -Out (Join-Path $EvidenceDir "s1_lax_determinism.json")

Invoke-Compare -Name "vn_determinism" `
    -A (Join-Path $EvidenceDir "s1_vn_run1.json") `
    -B (Join-Path $EvidenceDir "s1_vn_run2.json") `
    -Out (Join-Path $EvidenceDir "s1_vn_determinism.json")

Add-Log "[lcd_binary_determinism]"
$lcdPairs = @(
    @{ Model = "lax"; A = "s1_lcd_lax_run1"; B = "s1_lcd_lax_run2" },
    @{ Model = "vn"; A = "s1_lcd_vn_run1"; B = "s1_lcd_vn_run2" }
)
foreach ($pair in $lcdPairs) {
    $dirA = Join-Path $EvidenceDir $pair.A
    $dirB = Join-Path $EvidenceDir $pair.B
    $filesA = Get-ChildItem -LiteralPath $dirA -Filter "*.bin" | Sort-Object Name
    foreach ($fa in $filesA) {
        $fb = Join-Path $dirB $fa.Name
        if (-not (Test-Path -LiteralPath $fb)) {
            Add-Log ("MISSING {0}" -f $fa.Name)
            $failures.Add("lcd_binary_" + $pair.Model)
            continue
        }
        $ha = Get-Sha256Upper $fa.FullName
        $hb = Get-Sha256Upper $fb
        $equal = ($ha -eq $hb)
        Add-Log ("{0}/{1}: equal={2} sha256={3}" -f $pair.Model, $fa.Name, $equal, $ha)
        if (-not $equal) { $failures.Add("lcd_binary_" + $pair.Model) }
    }
}
Add-Log ""

$renderScript = Join-Path $base "ios_app_extracted\App calculadora cientifica ios\Tools\pruebas\render_lcd.py"
if (Test-Path -LiteralPath $renderScript) {
    Add-Log "[render_final_lcd]"
    $renderJobs = @(
        @{ Model = "lax"; Bin = (Join-Path $EvidenceDir "s1_lcd_lax_run1\lcd_lax_step4_60.bin"); Layout = "classwiz"; Png = (Join-Path $EvidenceDir "s1_lcd_lax_final.png"); Txt = (Join-Path $EvidenceDir "s1_render_lax.txt") },
        @{ Model = "vn"; Bin = (Join-Path $EvidenceDir "s1_lcd_vn_run1\lcd_vn_step4_60.bin"); Layout = "esplus"; Png = (Join-Path $EvidenceDir "s1_lcd_vn_final.png"); Txt = (Join-Path $EvidenceDir "s1_render_vn.txt") }
    )
    foreach ($job in $renderJobs) {
        if (-not (Test-Path -LiteralPath $job.Bin)) {
            Add-Log ("missing LCD dump: " + $job.Bin)
            $failures.Add("render_" + $job.Model)
            continue
        }
        $renderOutput = & $Python $renderScript $job.Bin $job.Layout $job.Png 2>&1
        $code = $LASTEXITCODE
        $renderOutput | Set-Content -LiteralPath $job.Txt -Encoding UTF8
        Add-Log ("{0}: exit_code={1} png={2} txt={3}" -f $job.Model, $code, $job.Png, $job.Txt)
        if ($code -ne 0) { $failures.Add("render_" + $job.Model) }
    }
    Add-Log ""
}

Add-Log "[step_summary]"
foreach ($model in @("lax", "vn")) {
    $jsonPath = Join-Path $EvidenceDir ("s1_{0}_run1.json" -f $model)
    $data = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    Add-Log ("{0}: status={1} mode={2} final_pc={3} final_sp={4} output_sha256={5}" -f `
        $model, $data.status, $data.mode, $data.final.pc, $data.final.sp, $data.output_sha256)
    foreach ($s in $data.steps) {
        Add-Log ("  step {0} key={1} wait={2} injected_at={3} total={4} pc={5} sp={6} lcd={7} nz={8}" -f `
            $s.step, $s.key.label, $s.wait_steps, $s.injected_at_instruction, $s.executed_total,
            $s.cpu.pc, $s.cpu.sp, $s.lcd.sha256, $s.lcd.non_zero_bytes)
    }
}
Add-Log ""

Add-Log "[result]"
if ($failures.Count -eq 0) {
    Add-Log "status: PASS"
} else {
    Add-Log ("status: FAIL (" + ($failures -join ", ") + ")")
}

$runLogPath = Join-Path $EvidenceDir "run_log_s1.txt"
Set-Content -LiteralPath $runLogPath -Value $runLog -Encoding UTF8
Write-Output "run log: $runLogPath"

Update-Sums

if ($failures.Count -ne 0) { exit 1 }
exit 0
