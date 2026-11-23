param(
    [string]$MonoRoot = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = 'Stop'

$srcSkins = Join-Path $MonoRoot 'assets\skins'
$srcRoms  = Join-Path $MonoRoot 'assets\roms'
$dstBase  = Join-Path $MonoRoot 'apps\android\assets_staged'
$dstSkins = Join-Path $dstBase 'skins'
$dstRoms  = Join-Path $dstBase 'roms'

New-Item -ItemType Directory -Force $dstSkins, $dstRoms | Out-Null

$skinFiles = @(
    'caratula_cg50.png',
    'caratula_cw2.png',
    'caratula_classwiz.png',
    'caratula_esplus.png'
)

$romFiles = @(
    'SystemROM.raw',
    'UserROM.raw',
    'C2ROM.raw',
    'official-ILRAM-FD800000-4000.bin',
    'cg50_warm_state.bin.gz',
    'fx991cw_rom.bin',
    'fx991cw_default_en.ram',
    'fx991lax.rom',
    'fx570vnplus.rom'
)

function Sync-FileVerified {
    param([string]$Src, [string]$Dst)
    if (-not (Test-Path -LiteralPath $Src)) {
        Write-Warning "Skipping missing optional asset: $Src"
        return
    }
    $srcHash = (Get-FileHash -LiteralPath $Src -Algorithm SHA256).Hash
    if (Test-Path -LiteralPath $Dst) {
        $dstHash = (Get-FileHash -LiteralPath $Dst -Algorithm SHA256).Hash
        if ($srcHash -eq $dstHash) { return }
    }
    Copy-Item -LiteralPath $Src -Destination $Dst -Force
    $verHash = (Get-FileHash -LiteralPath $Dst -Algorithm SHA256).Hash
    if ($verHash -ne $srcHash) {
        throw "SHA-256 verification failed after copy: $Dst"
    }
}

foreach ($f in $skinFiles) {
    Sync-FileVerified -Src (Join-Path $srcSkins $f) -Dst (Join-Path $dstSkins $f)
}
foreach ($f in $romFiles) {
    Sync-FileVerified -Src (Join-Path $srcRoms $f) -Dst (Join-Path $dstRoms $f)
}

Write-Host "Shared assets synced & SHA-256 verified -> $dstBase"
