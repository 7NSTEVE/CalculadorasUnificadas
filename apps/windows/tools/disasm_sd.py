import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

dll_path = r"CasioEmu\GCalAppIF.dll"
pe = pefile.PE(dll_path)
image_base = pe.OPTIONAL_HEADER.ImageBase
md = Cs(CS_ARCH_X86, CS_MODE_32)

export_dir = [exp for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols]
targets = ["SDCard", "RequestTask"]

def disasm_func(rva, max_len=120, name=""):
    print(f"\n--- Disassembly of {name} (RVA 0x{rva:08X}) ---")
    data = pe.get_data(rva, max_len)
    for ins in md.disasm(data, image_base + rva):
        print(f"  0x{ins.address:08X}: {ins.mnemonic:<8} {ins.op_str}")
        if ins.mnemonic in ("ret", "retf"):
            break

for exp in export_dir:
    name = exp.name.decode('utf-8') if exp.name else ""
    if any(t.lower() in name.lower() for t in targets):
        disasm_func(exp.address, 120, name)
