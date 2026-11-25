import pefile
p = r'E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris\Calculadora Unificada\build-verify\Release\runtime\ActivationFxMb.dll'
pe = pefile.PE(p, fast_load=False)
print('ImageBase:', hex(pe.OPTIONAL_HEADER.ImageBase))
print('Exports AcvFx*/IsActivated:')
if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        name = exp.name.decode('utf-8', 'replace') if exp.name else '(ord %d)' % exp.ordinal
        if 'AcvFx' in name or 'Activated' in name or 'License' in name or 'Disp' in name or 'ErrorDlg' in name:
            print(f'  {name:45s} RVA 0x{exp.address:04X}')
print()
# bytes en los RVAs de los 7 puntos
rvas = [(0x1580,'IsActivated'),(0x2180,'AcvFx_IsLicenseStatus'),(0x1D70,'AcvFx_IsActivationDialog'),
        (0x1A20,'AcvFx_IsActivationDialog2'),(0x21B0,'AcvFx_IsActivationErrorDlg'),
        (0x1F40,'AcvFx_DispActivationToolMenu'),(0x2010,'AcvFx_DispActivationToolActivation')]
data = pe.get_memory_mapped_image()
for rva, nm in rvas:
    b = data[rva:rva+8]
    print(f'{nm:38s} RVA 0x{rva:04X}: {b.hex(" ")}')
