import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe_path = r"CasioEmu\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
pe = pefile.PE(exe_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

# find FreeLibrary in IAT
free_lib_addr = None
for entry in pe.DIRECTORY_ENTRY_IMPORT:
    if "kernel32" in entry.dll.decode('utf-8').lower():
        for imp in entry.imports:
            name = imp.name.decode('utf-8') if imp.name else ""
            if name == "FreeLibrary":
                free_lib_addr = imp.address
                print(f"FreeLibrary IAT at 0x{free_lib_addr:08X}")

text_sec = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
sec_data = text_sec.get_data()
sec_va = image_base + text_sec.VirtualAddress

pattern = bytes([0xFF, 0x15, free_lib_addr & 0xFF, (free_lib_addr >> 8) & 0xFF, (free_lib_addr >> 16) & 0xFF, (free_lib_addr >> 24) & 0xFF])
pos = 0
found = 0
while True:
    idx = sec_data.find(pattern, pos)
    if idx == -1:
        break
    found += 1
    call_va = sec_va + idx
    print(f"Call to FreeLibrary at VA 0x{call_va:08X}")
    start = max(0, idx - 20)
    for ins in md.disasm(sec_data[start:idx+10], sec_va + start):
        mark = ">>> " if ins.address == call_va else "    "
        print(f"{mark}0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")
    pos = idx + 1
if found == 0:
    print("NO CALLS TO FreeLibrary FOUND IN .text!")
