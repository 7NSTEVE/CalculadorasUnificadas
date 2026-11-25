import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

dll_path = r"CasioEmu\GCalAppIF.dll"
pe = pefile.PE(dll_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

def disasm_range(start_rva, end_rva):
    data = pe.get_data(start_rva, end_rva - start_rva)
    for ins in md.disasm(data, image_base + start_rva):
        print(f"  0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")

print("=== Disassembly of func_100036B0 from 0x10003740 to 0x10003950 ===")
disasm_range(0x3740, 0x3960)

print("\n=== Disassembly of callback 0x10003690 ===")
disasm_range(0x3690, 0x36B0)
