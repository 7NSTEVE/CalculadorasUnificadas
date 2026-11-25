import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

dll_path = r"CasioEmu\CASIO GCSim.dll"
pe = pefile.PE(dll_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

print(f"CASIO GCSim.dll ImageBase: 0x{image_base:08X}")
text_sec = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
sec_data = text_sec.get_data()
sec_va = image_base + text_sec.VirtualAddress

offset = 0x1200
data = pe.get_data(offset, 150)
for ins in md.disasm(data, image_base + offset):
    mark = ">>> " if ins.address == image_base + 0x1243 else "    "
    print(f"{mark}0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")

