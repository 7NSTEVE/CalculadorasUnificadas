import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe_path = r"CasioEmu\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
pe = pefile.PE(exe_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

text_sec = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
sec_data = text_sec.get_data()
sec_va = image_base + text_sec.VirtualAddress

offset = 0x0040D600 - sec_va
data = sec_data[offset:offset + 200]
for ins in md.disasm(data, 0x0040D600):
    print(f"  0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")
    if ins.mnemonic == "ret":
        break
