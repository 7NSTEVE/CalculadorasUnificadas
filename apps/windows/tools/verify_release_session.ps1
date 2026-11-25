param(
    [string]$ReleaseDir = "d:\OneDrive\Desktop\Calculadoras (blanca y gris)\Calculadora Unificada - Core Oficial\build-win32\Release"
)

$ErrorActionPreference = "Stop"

Write-Host "=== VERIFYING CALCULADORA UNIFICADA RELEASE ==="

# 1. Hashes
$exePath = Join-Path $ReleaseDir "CalculadoraUnificada.exe"
$dllPath = Join-Path $ReleaseDir "runtime\ActivationFxMb.dll"

if (-not (Test-Path $exePath)) {
    throw "EXE not found: $exePath"
}
if (-not (Test-Path $dllPath)) {
    throw "DLL not found: $dllPath"
}

$exeHash = (Get-FileHash -Algorithm SHA256 $exePath).Hash
$dllHash = (Get-FileHash -Algorithm SHA256 $dllPath).Hash

Write-Host "CalculadoraUnificada.exe SHA256: $exeHash"
Write-Host "ActivationFxMb.dll SHA256:       $dllHash"

# 2. Cleanup any old processes and logs
Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$logPath = Join-Path $ReleaseDir "calc_debug.log"
if (Test-Path $logPath) {
    Remove-Item $logPath -Force
}

# 3. Launch process
Set-Location -LiteralPath $ReleaseDir
$proc = Start-Process -FilePath $exePath -WorkingDirectory $ReleaseDir -PassThru
Write-Host "Process launched. PID=$($proc.Id)"

# Wait for initialization
Start-Sleep -Seconds 6

$proc.Refresh()
if ($proc.HasExited) {
    throw "Process exited prematurely with code $($proc.ExitCode)"
}

Write-Host "Process is running (PID=$($proc.Id), Responding=$($proc.Responding))"

# Check log
if (Test-Path $logPath) {
    Write-Host "=== calc_debug.log ==="
    Get-Content $logPath | ForEach-Object { Write-Host "LOG: $_" }
} else {
    Write-Host "WARNING: calc_debug.log not found yet"
}

# 4. Check Window
$mainHwnd = $proc.MainWindowHandle
Write-Host "MainWindowHandle: 0x$([System.Convert]::ToString($mainHwnd.ToInt64(), 16))"
Write-Host "MainWindowTitle:  $($proc.MainWindowTitle)"

# 5. Capture Initial Framebuffer using InspectGui.exe
$screenInit = Join-Path $ReleaseDir "capture_init.png"
Write-Host "Capturing initial window to $screenInit..."
& ".\InspectGui.exe" $proc.Id "capture" $screenInit

# 6. Switch to CG50 Mode (ID_BTN_MODE_GRAPH = 1014)
Write-Host "Switching to Solo CG50 mode (1014)..."
& ".\InspectGui.exe" $proc.Id "mode" "1014"
Start-Sleep -Seconds 2

# 7. Send test key: 7 (VK_NUMPAD7 / 0x37 = '7')
Write-Host "Sending Key '7' (0x37 = 55)..."
& ".\InspectGui.exe" $proc.Id "vk" "55"
Start-Sleep -Milliseconds 500

# Send '+' (VK_ADD = 107)
Write-Host "Sending Key '+' (VK_ADD = 107)..."
& ".\InspectGui.exe" $proc.Id "vk" "107"
Start-Sleep -Milliseconds 500

# Send '5' (VK_NUMPAD5 / 0x35 = '5' = 53)
Write-Host "Sending Key '5' (0x35 = 53)..."
& ".\InspectGui.exe" $proc.Id "vk" "53"
Start-Sleep -Milliseconds 500

# Send EXE (VK_RETURN = 13)
Write-Host "Sending Key 'EXE' (VK_RETURN = 13)..."
& ".\InspectGui.exe" $proc.Id "vk" "13"
Start-Sleep -Seconds 2

# 8. Capture Post-Input Framebuffer
$screenKeys = Join-Path $ReleaseDir "capture_after_keys.png"
Write-Host "Capturing post-input window to $screenKeys..."
& ".\InspectGui.exe" $proc.Id "capture" $screenKeys

# Also capture full 3-calculator view (mode 1010)
Write-Host "Switching to All 3 mode (1010)..."
& ".\InspectGui.exe" $proc.Id "mode" "1010"
Start-Sleep -Seconds 2
$screenAll = Join-Path $ReleaseDir "capture_all_three.png"
& ".\InspectGui.exe" $proc.Id "capture" $screenAll

# 9. Clean shutdown
Write-Host "Closing process cleanly..."
$closedCleanly = $proc.CloseMainWindow()
Write-Host "CloseMainWindow returned: $closedCleanly"

$waitTimeout = 5
$stopped = $proc.WaitForExit($waitTimeout * 1000)
if (-not $stopped) {
    Write-Host "Process did not exit within $waitTimeout s, calling Stop-Process"
    Stop-Process -Id $proc.Id -Force
} else {
    Write-Host "Process exited cleanly with code: $($proc.ExitCode)"
}

# 10. Check no orphan processes remain
Start-Sleep -Seconds 1
$remaining = Get-Process CalculadoraUnificada -ErrorAction SilentlyContinue
if ($remaining) {
    Write-Host "WARNING: CalculadoraUnificada still running: $($remaining.Count) instances"
    $remaining | Stop-Process -Force
} else {
    Write-Host "CONFIRMED: No CalculadoraUnificada processes left running."
}

# 11. Final verification of generated captures
if (Test-Path $screenInit) {
    $initSize = (Get-Item $screenInit).Length
    $initHash = (Get-FileHash -Algorithm SHA256 $screenInit).Hash
    Write-Host "capture_init.png: Size=$initSize bytes, SHA256=$initHash"
}
if (Test-Path $screenKeys) {
    $keysSize = (Get-Item $screenKeys).Length
    $keysHash = (Get-FileHash -Algorithm SHA256 $screenKeys).Hash
    Write-Host "capture_after_keys.png: Size=$keysSize bytes, SHA256=$keysHash"
}
if (Test-Path $screenAll) {
    $allSize = (Get-Item $screenAll).Length
    $allHash = (Get-FileHash -Algorithm SHA256 $screenAll).Hash
    Write-Host "capture_all_three.png: Size=$allSize bytes, SHA256=$allHash"
}

Write-Host "=== TEST RUN COMPLETE ==="
