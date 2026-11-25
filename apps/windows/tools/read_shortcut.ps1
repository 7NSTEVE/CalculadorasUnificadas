$ErrorActionPreference = 'Stop'
$sh = New-Object -ComObject WScript.Shell
$lnk = $sh.CreateShortcut('C:\ProgramData\Microsoft\Windows\Start Menu\Programs\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG Manager PLUS Subscription for fx-CG50series.lnk')
Write-Output ("Target    = " + $lnk.TargetPath)
Write-Output ("Arguments = [" + $lnk.Arguments + "]")
Write-Output ("WorkDir   = " + $lnk.WorkingDirectory)
