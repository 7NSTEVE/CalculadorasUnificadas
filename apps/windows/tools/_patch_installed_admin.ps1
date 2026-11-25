$py = 'D:\DevTools\Python311\python.exe'
$script = 'E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris\Calculadora Unificada\tools\patch_activationfxmb.py'
$dll = 'C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\ActivationFxMb.dll'
$out = 'E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris\Calculadora Unificada\build-verify\patch_admin_result.txt'
& $py $script $dll 2>&1 | Out-File -FilePath $out -Encoding UTF8
Write-Output ('exit=' + $LASTEXITCODE) | Out-File -FilePath $out -Encoding UTF8 -Append
