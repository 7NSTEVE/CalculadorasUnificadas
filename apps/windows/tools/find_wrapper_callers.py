import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

exe_path = r"CasioEmu\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe"
pe = pefile.PE(exe_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

text_sec = [s for s in pe.sections if s.Name.startswith(b'.text')][0]
sec_data = text_sec.get_data()
sec_va = image_base + text_sec.VirtualAddress

# Search for calls to 0x0040D8F1 (relative call: E8 rel32)
# The function starts at e.g. 0x0040D8C0 or 0x0040D8D0? Let's check prologue
# Look backwards from 0x40D8F1
for p in range(0x40D8F1, 0x40D850, -1):
    idx = p - sec_va
    if sec_data[idx:idx+3] == bytes([0x55, 0x8B, 0xEC]): # push ebp; mov ebp, esp
        func_entry = p
        print(f"Wrapper function entry: 0x{func_entry:08X}")
        break

# Now search for calls to func_entry
for i in range(len(sec_data) - 5):
    if sec_data[i] == 0xE8:
        rel = int.from_bytes(sec_data[i+1:i+5], byteorder='little', signed=True)
        target = sec_va + i + 5 + rel
        if target == func_entry:
            call_va = sec_va + i
            print(f"Call to wrapper at VA 0x{call_va:08X}")
            # disasm 10 instructions before call
            start = max(0, i - 30)
            for ins in md.disasm(sec_data[start:i+5], sec_va + start):
                mark = ">>> " if ins.address == call_va else "    "
                print(f"{mark}0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")

