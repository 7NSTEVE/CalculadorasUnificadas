param(
    [Parameter(Mandatory=$true)][string]$TargetDir,
    [string]$MonoRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$BuildDir = "",
    [switch]$IncludeRoms
)
$ErrorActionPreference = 'Stop'

# Local portable staging script ONLY: copies built native artifacts, skins,
# certification reports and generates a SHA-256 manifest. Never runs git push,
# never modifies remote visibility.

$resolvedMono = (Resolve-Path $MonoRoot).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $resolvedMono "build-core-host\Release"
}

New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
$stageBin  = Join-Path $TargetDir "bin"
$stageDocs = Join-Path $TargetDir "docs"
$stageCert = Join-Path $TargetDir "certification_v47"
New-Item -ItemType Directory -Force -Path $stageBin, $stageDocs, $stageCert | Out-Null

$copied = @()

function Copy-StageFile([string]$src, [string]$dstDir) {
    if (Test-Path -LiteralPath $src -PathType Leaf) {
        $dst = Join-Path $dstDir (Split-Path -Leaf $src)
        Copy-Item -LiteralPath $src -Destination $dst -Force
        $hash = (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash
        $size = (Get-Item -LiteralPath $dst).Length
        return [ordered]@{
            relative_path = $dst.Substring($TargetDir.Length).TrimStart('\','/').Replace('\','/')
            bytes = $size
            sha256 = $hash
        }
    }
    return $null
}

# 1. Native Release binaries (host FFI DLL + optional certification runners)
$binCandidates = @(
    "casio_native.dll",
    "cg50_core_c_api_contract_test.exe",
    "cg50_sh4a_isa_test.exe",
    "cg50_sh7305_hw_test.exe",
    "cg50_hw_silicon_100_test.exe",
    "cg50_register_cert_lab.exe",
    "cg50_combined_hw_stress.exe"
)
foreach ($f in $binCandidates) {
    $entry = Copy-StageFile (Join-Path $BuildDir $f) $stageBin
    if ($null -ne $entry) { $copied += $entry }
}

# 2. Documentation & v47 certification deliverables
$docCandidates = @(
    (Join-Path $resolvedMono "README.md"),
    (Join-Path $resolvedMono "MATRIZ-COMPATIBILIDAD.md"),
    (Join-Path $resolvedMono "tests\cg50_validation\reports\AUDIT_AUTHENTICITY.md"),
    (Join-Path $resolvedMono "tests\cg50_validation\CG50_VALIDATION_MASTER.json"),
    (Join-Path $resolvedMono "tests\cg50_validation\CG50_HARDWARE_MASTER.json")
)
foreach ($p in $docCandidates) {
    $entry = Copy-StageFile $p $stageDocs
    if ($null -ne $entry) { $copied += $entry }
}

$certDir = Join-Path $resolvedMono "tests\cg50_validation\certification_v47"
if (Test-Path -LiteralPath $certDir -PathType Container) {
    foreach ($cf in Get-ChildItem -LiteralPath $certDir -File) {
        if ($cf.Extension -in @('.json', '.csv', '.md')) {
            $entry = Copy-StageFile $cf.FullName $stageCert
            if ($null -ne $entry) { $copied += $entry }
        }
    }
}

# 3. Skins (UI faceplates)
$skinsSrc = Join-Path $resolvedMono "apps\android\assets_staged\skins"
if (Test-Path -LiteralPath $skinsSrc -PathType Container) {
    $stageSkins = Join-Path $TargetDir "skins"
    New-Item -ItemType Directory -Force -Path $stageSkins | Out-Null
    foreach ($sf in Get-ChildItem -LiteralPath $skinsSrc -File) {
        $entry = Copy-StageFile $sf.FullName $stageSkins
        if ($null -ne $entry) { $copied += $entry }
    }
}

# 4. Optional local-only ROM bundle (OFF by default to prevent accidental redistribution)
if ($IncludeRoms) {
    $romsSrc = Join-Path $resolvedMono "assets\roms"
    if (Test-Path -LiteralPath $romsSrc -PathType Container) {
        $stageRoms = Join-Path $TargetDir "roms_local_only"
        New-Item -ItemType Directory -Force -Path $stageRoms | Out-Null
        foreach ($rf in Get-ChildItem -LiteralPath $romsSrc -File) {
            $entry = Copy-StageFile $rf.FullName $stageRoms
            if ($null -ne $entry) { $copied += $entry }
        }
    }
}

$manifest = [ordered]@{
    schema_version = "1.0.0"
    staged_utc     = (Get-Date).ToUniversalTime().ToString("o")
    source_root    = $resolvedMono
    target_dir     = (Resolve-Path $TargetDir).Path
    includes_roms  = [bool]$IncludeRoms
    git_push_performed = $false
    file_count     = $copied.Count
    files          = $copied
}
$manifestPath = Join-Path $TargetDir "PORTABLE_MANIFEST.json"
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Host "[PORTABLE-STAGE] Staged $($copied.Count) files into $TargetDir (IncludeRoms=$IncludeRoms, no git push performed)."
Write-Host "[PORTABLE-STAGE] Manifest: $manifestPath"
