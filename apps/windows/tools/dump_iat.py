import pefile

dll_path = r"CasioEmu\GCalAppIF.dll"
pe = pefile.PE(dll_path)
image_base = pe.OPTIONAL_HEADER.ImageBase

print("=== Import Table for GCalAppIF.dll ===")
for entry in pe.DIRECTORY_ENTRY_IMPORT:
    dll_name = entry.dll.decode('utf-8')
    print(f"\nDLL: {dll_name}")
    for imp in entry.imports:
        addr = imp.address
        name = imp.name.decode('utf-8') if imp.name else f"ord_{imp.ordinal}"
        print(f"  [0x{addr:08X}] {name}")
