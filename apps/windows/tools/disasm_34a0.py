import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

dll_path = r"CasioEmu\GCalAppIF.dll"
pe = pefile.PE(dll_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

def disasm_func(rva, max_len=300, name=""):
    print(f"\n--- Disassembly of {name} (RVA 0x{rva:08X}, VA 0x{image_base + rva:08X}) ---")
    data = pe.get_data(rva, max_len)
    for ins in md.disasm(data, image_base + rva):
        print(f"  0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")

disasm_func(0x34A0, 200, "func_100034A0")
