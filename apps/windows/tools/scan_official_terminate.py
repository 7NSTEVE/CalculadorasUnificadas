import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
import os

exe_path = r"CasioEmu\fx-CG Manager PLUS Subscription for fx-CG50series.exe"
if not os.path.exists(exe_path):
    # Search for it
    for root, dirs, files in os.walk(r"E:\Descompilados\11_Otros\Emuladores\Calculadoras_blanca_y_gris\Calculadora Unificada"):
        for f in files:
            if f.endswith(".exe") and "Manager" in f:
                exe_path = os.path.join(root, f)
                break

print("Checking exe:", exe_path)
pe = pefile.PE(exe_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

# Find imports from GCalAppIF.dll
gcai_imports = {}
for entry in pe.DIRECTORY_ENTRY_IMPORT:
    if "gcal" in entry.dll.decode('utf-8').lower():
        for imp in entry.imports:
            name = imp.name.decode('utf-8') if imp.name else f"ord_{imp.ordinal}"
            gcai_imports[imp.address] = name
            print(f"Imported GCal symbol: {name} at IAT 0x{imp.address:08X}")

# Search text section for calls/references to TerminateEmulator / TerminateResidentTask
text_sec = None
for s in pe.sections:
    if s.Name.startswith(b'.text'):
        text_sec = s
        break

if text_sec:
    sec_data = text_sec.get_data()
    sec_va = image_base + text_sec.VirtualAddress
    print(f"Scanning .text: VA 0x{sec_va:08X}, size {len(sec_data)}")
    
    # Let's search for calls to IAT pointers
    for iat_addr, name in gcai_imports.items():
        if "terminate" in name.lower():
            # FF 15 <iat_addr little endian>
            pattern = bytes([0xFF, 0x15, iat_addr & 0xFF, (iat_addr >> 8) & 0xFF, (iat_addr >> 16) & 0xFF, (iat_addr >> 24) & 0xFF])
            pos = 0
            while True:
                idx = sec_data.find(pattern, pos)
                if idx == -1:
                    break
                call_va = sec_va + idx
                print(f"\nFound call to {name} at VA 0x{call_va:08X}")
                # Disassemble surrounding 100 bytes
                disasm_start = max(0, idx - 40)
                disasm_data = sec_data[disasm_start:idx + 40]
                for ins in md.disasm(disasm_data, sec_va + disasm_start):
                    mark = ">>> " if ins.address == call_va else "    "
                    print(f"{mark}0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")
                pos = idx + 1
