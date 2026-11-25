import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

dll_path = r"CasioEmu\CASIO GCSim.dll"
pe = pefile.PE(dll_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

text_sec = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
sec_data = text_sec.get_data()
sec_va = image_base + text_sec.VirtualAddress

# Search for references to 0x1000509c
target = image_base + 0x509c
target_bytes = target.to_bytes(4, 'little')

print(f"Target address: 0x{target:08X}")
pos = 0
while True:
    idx = sec_data.find(target_bytes, pos)
    if idx == -1:
        break
    va = sec_va + idx
    print(f"\nMatch at VA 0x{va:08X}")
    start = max(0, idx - 15)
    for ins in md.disasm(sec_data[start:idx+15], sec_va + start):
        mark = ">>> " if any(f"0x{target:x}" in ins.op_str.lower() for _ in [1]) else "    "
        print(f"{mark}0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")
    pos = idx + 1
